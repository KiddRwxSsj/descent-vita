#include <string.h>

#include <psp2/kernel/threadmgr.h>

#define TSF_IMPLEMENTATION
#include "tsf.h"

#include "types.h"
#include "mixer.h"

#define MUSIC_RATE		44100
#define MAX_TRACKS		32
#define MAX_VOICES		48

// events are applied between blocks, 64 frames is under 1.5ms of slop
#define BLOCK_FRAMES	64

typedef struct track {
	const ubyte *pos, *end;
	unsigned int next;
} track;

static tsf *synth;
static float volume;

static track tracks[MAX_TRACKS];
static const ubyte *track_start[MAX_TRACKS];
static int num_tracks;
static int playing, looping;
static double frames_per_tick, song_frames;

static SceKernelLwMutexWork music_lock __attribute__((aligned(8)));

// HMI deltas run low 7 bits first and flag the last byte with 0x80,
// the opposite of a standard MIDI varlen
static unsigned int read_delta(const ubyte **p, const ubyte *end)
{
	unsigned int v = 0;
	int shift = 0;

	while (*p < end && !(**p & 0x80)) {
		v |= (unsigned int)(**p) << shift;
		shift += 7;
		(*p)++;
	}
	if (*p < end) {
		v |= (unsigned int)(**p & 0x7f) << shift;
		(*p)++;
	}
	return v;
}

static void reset_channels(void)
{
	int ch;

	tsf_reset(synth);
	for (ch = 0; ch < 16; ch++) {
		tsf_channel_set_presetnumber(synth, ch, 0, ch == 9);
		tsf_channel_midi_control(synth, ch, 7, 100);
	}
}

static void rewind_song(void)
{
	int i;

	for (i = 0; i < num_tracks; i++) {
		tracks[i].pos = track_start[i];
		tracks[i].next = read_delta(&tracks[i].pos, tracks[i].end);
	}
	song_frames = 0;
}

static void play_event(track *t)
{
	const ubyte *p = t->pos;
	int ch = p[0] & 15;

	if (p[0] == 0xff) {
		if (p + 3 > t->end || p[1] == 0x2f) {
			t->pos = t->end;
			return;
		}
		t->pos = p + 3 + p[2];
	} else switch (p[0] & 0xf0) {
	case 0x80:
		tsf_channel_note_off(synth, ch, p[1]);
		t->pos = p + 3;
		break;
	case 0x90:
		if (p[2])
			tsf_channel_note_on(synth, ch, p[1], p[2] / 127.0f);
		else
			tsf_channel_note_off(synth, ch, p[1]);
		t->pos = p + 3;
		break;
	case 0xb0:
		tsf_channel_midi_control(synth, ch, p[1], p[2]);
		t->pos = p + 3;
		break;
	case 0xe0:
		tsf_channel_set_pitchwheel(synth, ch, p[1] | (p[2] << 7));
		t->pos = p + 3;
		break;
	case 0xc0:
		tsf_channel_set_presetnumber(synth, ch, p[1], ch == 9);
		t->pos = p + 2;
		break;
	case 0xa0:
		t->pos = p + 3;
		break;
	case 0xd0:
		t->pos = p + 2;
		break;
	default:
		// no running status in HMP, a data byte here means the track is junk
		t->pos = t->end;
		return;
	}

	if (t->pos < t->end)
		t->next += read_delta(&t->pos, t->end);
}

// fires every event due by the current song position, returns 0 once all tracks ran out
static int run_events(void)
{
	int i, alive = 0;
	track *t;

	for (i = 0; i < num_tracks; i++) {
		t = &tracks[i];
		while (t->pos < t->end && t->next * frames_per_tick <= song_frames)
			play_event(t);
		if (t->pos < t->end)
			alive = 1;
	}
	return alive;
}

int music_init(const char *soundfont)
{
	sceKernelCreateLwMutex(&music_lock, "music_lock", 0, 0, NULL);

	synth = tsf_load_filename(soundfont);
	if (!synth)
		return 0;

	tsf_set_output(synth, TSF_STEREO_INTERLEAVED, MUSIC_RATE, 0);
	tsf_set_max_voices(synth, MAX_VOICES);
	reset_channels();
	return 1;
}

// Same layout DXX-Rebirth reads: track count at 0x30, timebase at 0x38,
// tracks from 0x308 each with a 12 byte header. Track 0 carries no notes.
// The ticks come out at 1.6 per timebase unit against a 0x188000us beat.
int music_start(const ubyte *data, int size, int loop)
{
	const ubyte *p, *end = data + size;
	int i, n, len, timebase;

	if (!synth || size < 0x308 || memcmp(data, "HMIMIDIP", 8))
		return 0;

	n = data[0x30] | (data[0x31] << 8) | (data[0x32] << 16) | (data[0x33] << 24);
	timebase = data[0x38] | (data[0x39] << 8) | (data[0x3a] << 16) | (data[0x3b] << 24);
	if (n < 1 || n > MAX_TRACKS || timebase <= 0)
		return 0;

	music_stop();

	sceKernelLockLwMutex(&music_lock, 1, NULL);

	num_tracks = 0;
	p = data + 0x308;
	for (i = 0; i < n && p + 12 <= end; i++) {
		len = p[4] | (p[5] << 8) | (p[6] << 16) | (p[7] << 24);
		if (len < 12 || p + len > end)
			break;
		if (i > 0) {
			track_start[num_tracks] = p + 12;
			tracks[num_tracks].end = p + len;
			num_tracks++;
		}
		p += len;
	}

	frames_per_tick = MUSIC_RATE * 1.605632 / (int)(timebase * 1.6);
	looping = loop;
	reset_channels();
	rewind_song();
	playing = num_tracks > 0;

	sceKernelUnlockLwMutex(&music_lock, 1);
	return playing;
}

void music_stop(void)
{
	if (!synth)
		return;

	sceKernelLockLwMutex(&music_lock, 1, NULL);
	playing = 0;
	num_tracks = 0;
	tsf_reset(synth);
	sceKernelUnlockLwMutex(&music_lock, 1);
}

// 0..127, same scale the SOS driver took
void music_set_volume(int v)
{
	volume = v / 127.0f;
}

void music_mix(int *out, int frames)
{
	static float buf[BLOCK_FRAMES * 2];
	int i, n;
	float gain;

	if (volume <= 0 || dxa_mix(out, frames, volume))
		return;
	if (!synth || !playing)
		return;

	sceKernelLockLwMutex(&music_lock, 1, NULL);

	gain = volume * 24000.0f;
	while (playing && frames > 0) {
		n = frames < BLOCK_FRAMES ? frames : BLOCK_FRAMES;

		if (!run_events()) {
			if (looping) {
				rewind_song();
				run_events();
			} else
				playing = 0;
		}

		tsf_render_float(synth, buf, n, 0);
		for (i = 0; i < n * 2; i++)
			out[i] += (int)(buf[i] * gain);

		song_frames += n;
		out += n * 2;
		frames -= n;
	}

	sceKernelUnlockLwMutex(&music_lock, 1);
}
