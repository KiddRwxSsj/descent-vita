#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <zlib.h>

#include <psp2/kernel/threadmgr.h>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "vorbis.h"

#include "types.h"
#include "mixer.h"

#define MAX_ENTRIES		64
#define DXA_RATE		44100
#define DECODE_FRAMES	512

typedef struct entry {
	char name[16];
	int method, csize, size, offset;
} entry;

static char pack[256];
static entry entries[MAX_ENTRIES];
static int num_entries;

static SceKernelLwMutexWork dxa_lock __attribute__((aligned(8)));
static SceUID loader_sema;

static volatile int want_serial, want_entry = -1, want_loop;

static stb_vorbis *song;
static ubyte *song_data;
static int song_loop, song_ended;

static int zip16(const ubyte *p)
{
	return p[0] | (p[1] << 8);
}

static int zip32(const ubyte *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

// a .dxa is a plain zip, only the central directory at the end matters
static int read_directory(FILE *f)
{
	static ubyte tail[22 + 0xffff];
	ubyte *p, *cd, *end;
	long size, start;
	int i, n, len, cd_size, cd_offset, name_len;

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	start = size > (long)sizeof(tail) ? size - (long)sizeof(tail) : 0;
	fseek(f, start, SEEK_SET);
	len = fread(tail, 1, size - start, f);

	for (p = tail + len - 22; p >= tail; p--)
		if (zip32(p) == 0x06054b50)
			break;
	if (p < tail)
		return 0;

	n = zip16(p + 10);
	cd_size = zip32(p + 12);
	cd_offset = zip32(p + 16);

	cd = malloc(cd_size);
	if (!cd)
		return 0;
	fseek(f, cd_offset, SEEK_SET);
	if (fread(cd, 1, cd_size, f) != cd_size) {
		free(cd);
		return 0;
	}

	end = cd + cd_size;
	for (p = cd, i = 0; i < n && p + 46 <= end && zip32(p) == 0x02014b50; i++) {
		entry *e = &entries[num_entries];

		name_len = zip16(p + 28);
		e->method = zip16(p + 10);
		e->csize = zip32(p + 20);
		e->size = zip32(p + 24);
		e->offset = zip32(p + 42);
		if (num_entries < MAX_ENTRIES && name_len < sizeof(e->name) && (e->method == 0 || e->method == 8)) {
			memcpy(e->name, p + 46, name_len);
			e->name[name_len] = 0;
			if (name_len > 4 && !strcasecmp(e->name + name_len - 4, ".ogg"))
				num_entries++;
		}
		p += 46 + name_len + zip16(p + 30) + zip16(p + 32);
	}

	free(cd);
	return num_entries;
}

static ubyte *read_entry(const entry *e)
{
	static ubyte in[64 * 1024];
	ubyte hdr[30], *out;
	z_stream z;
	FILE *f;
	int left, n, ok;

	f = fopen(pack, "rb");
	if (!f)
		return NULL;

	out = malloc(e->size);
	fseek(f, e->offset, SEEK_SET);
	if (!out || fread(hdr, 1, 30, f) != 30 || zip32(hdr) != 0x04034b50) {
		free(out);
		fclose(f);
		return NULL;
	}
	fseek(f, e->offset + 30 + zip16(hdr + 26) + zip16(hdr + 28), SEEK_SET);

	if (e->method == 0) {
		ok = fread(out, 1, e->size, f) == e->size;
	} else {
		memset(&z, 0, sizeof(z));
		inflateInit2(&z, -MAX_WBITS);
		z.next_out = out;
		z.avail_out = e->size;
		left = e->csize;
		ok = 0;
		while (left > 0) {
			n = fread(in, 1, left < sizeof(in) ? left : sizeof(in), f);
			if (n <= 0)
				break;
			left -= n;
			z.next_in = in;
			z.avail_in = n;
			n = inflate(&z, Z_NO_FLUSH);
			if (n == Z_STREAM_END) {
				ok = 1;
				break;
			}
			if (n != Z_OK)
				break;
		}
		inflateEnd(&z);
	}

	fclose(f);
	if (!ok) {
		free(out);
		return NULL;
	}
	return out;
}

// songs run up to ~17MB, so reading and inflating them happens here and not in the game or mixer thread
static int loader_main(SceSize args, void *argp)
{
	stb_vorbis *v;
	ubyte *data;
	int serial, e, loop, err;

	for (;;) {
		sceKernelWaitSema(loader_sema, 1, NULL);

		serial = want_serial;
		e = want_entry;
		loop = want_loop;
		if (e < 0)
			continue;

		data = read_entry(&entries[e]);
		if (!data)
			continue;

		v = stb_vorbis_open_memory(data, entries[e].size, &err, NULL);
		if (v && stb_vorbis_get_info(v).sample_rate != DXA_RATE) {
			stb_vorbis_close(v);
			v = NULL;
		}

		sceKernelLockLwMutex(&dxa_lock, 1, NULL);
		if (v && serial == want_serial && !song) {
			song = v;
			song_data = data;
			song_loop = loop;
			song_ended = 0;
			v = NULL;
			data = NULL;
		}
		sceKernelUnlockLwMutex(&dxa_lock, 1);

		if (v)
			stb_vorbis_close(v);
		free(data);
	}
	return 0;
}

int dxa_init(void)
{
	struct dirent *d;
	DIR *dir;
	FILE *f;
	SceUID thid;
	int len;

	dir = opendir(".");
	if (!dir)
		return 0;
	while ((d = readdir(dir)) != NULL) {
		len = strlen(d->d_name);
		if (len > 4 && len < sizeof(pack) && !strcasecmp(d->d_name + len - 4, ".dxa")) {
			strcpy(pack, d->d_name);
			break;
		}
	}
	closedir(dir);
	if (!pack[0])
		return 0;

	f = fopen(pack, "rb");
	if (!f)
		return 0;
	read_directory(f);
	fclose(f);
	if (!num_entries)
		return 0;

	sceKernelCreateLwMutex(&dxa_lock, "dxa_lock", 0, 0, NULL);
	loader_sema = sceKernelCreateSema("dxa_loader", 0, 0, 16, NULL);
	thid = sceKernelCreateThread("dxa_loader", loader_main, 0x10000100 + 10, 0x10000, 0, SCE_KERNEL_CPU_MASK_USER_2, NULL);
	if (thid < 0)
		return num_entries = 0;
	sceKernelStartThread(thid, 0, NULL);
	return 1;
}

void dxa_stop(void)
{
	stb_vorbis *v;
	ubyte *data;

	if (!num_entries)
		return;

	sceKernelLockLwMutex(&dxa_lock, 1, NULL);
	want_serial++;
	want_entry = -1;
	v = song;
	data = song_data;
	song = NULL;
	song_data = NULL;
	sceKernelUnlockLwMutex(&dxa_lock, 1);

	if (v)
		stb_vorbis_close(v);
	free(data);
}

// game01.hmp plays game01.ogg from the pack; 0 means there's no recording and the midi should play
int dxa_play(const char *filename, int loop)
{
	char name[16];
	char *dot;
	int i;

	if (!num_entries || !filename)
		return 0;

	strncpy(name, filename, sizeof(name) - 5);
	name[sizeof(name) - 5] = 0;
	dot = strchr(name, '.');
	if (dot)
		*dot = 0;
	strcat(name, ".ogg");

	for (i = 0; i < num_entries; i++)
		if (!strcasecmp(entries[i].name, name))
			break;
	if (i == num_entries)
		return 0;

	dxa_stop();
	want_entry = i;
	want_loop = loop;
	sceKernelSignalSema(loader_sema, 1);
	return 1;
}

int dxa_mix(int *out, int frames, float volume)
{
	static short buf[DECODE_FRAMES * 2];
	int i, n, gain, rewound = 0;

	if (!num_entries)
		return 0;

	sceKernelLockLwMutex(&dxa_lock, 1, NULL);
	if (!song) {
		sceKernelUnlockLwMutex(&dxa_lock, 1);
		return want_entry >= 0;
	}

	// a bit under full scale so it sits at the same level as the soundfont music
	gain = (int)(volume * 192);
	while (frames > 0 && !song_ended) {
		n = stb_vorbis_get_samples_short_interleaved(song, 2, buf, (frames < DECODE_FRAMES ? frames : DECODE_FRAMES) * 2);
		if (n <= 0) {
			if (!song_loop || rewound) {
				song_ended = 1;
				break;
			}
			stb_vorbis_seek_start(song);
			rewound = 1;
			continue;
		}
		rewound = 0;
		for (i = 0; i < n * 2; i++)
			out[i] += (buf[i] * gain) >> 8;
		out += n * 2;
		frames -= n;
	}

	sceKernelUnlockLwMutex(&dxa_lock, 1);
	return 1;
}
