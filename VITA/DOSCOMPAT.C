#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

#include <psp2/kernel/threadmgr.h>

#include "doscompat.h"
#include "vita.h"

int int386(int intno, union REGS *in, union REGS *out)
{
	if (out != in)
		memcpy(out, in, sizeof(union REGS));
	out->x.cflag = 1;
	return 0;
}

int int386x(int intno, union REGS *in, union REGS *out, struct SREGS *seg)
{
	return int386(intno, in, out);
}

void segread(struct SREGS *seg)
{
	memset(seg, 0, sizeof(*seg));
}

unsigned char inp(unsigned short port)
{
	return vita_dac_in(port);
}

unsigned short inpw(unsigned short port)
{
	return vita_dac_in(port);
}

unsigned char outp(unsigned short port, unsigned char value)
{
	vita_dac_out(port, value);
	return value;
}

unsigned short outpw(unsigned short port, unsigned short value)
{
	vita_dac_out(port, value & 0xff);
	vita_dac_out(port + 1, value >> 8);
	return value;
}

void _disable(void)
{
}

void _enable(void)
{
}

void _harderr(void *handler)
{
}

char *strupr(char *s)
{
	char *p;

	for (p = s; *p; p++)
		*p = toupper((unsigned char)*p);
	return s;
}

char *strlwr(char *s)
{
	char *p;

	for (p = s; *p; p++)
		*p = tolower((unsigned char)*p);
	return s;
}

char *strrev(char *s)
{
	char *a = s, *b = s + strlen(s) - 1, t;

	while (a < b) {
		t = *a; *a++ = *b; *b-- = t;
	}
	return s;
}

char *ultoa(unsigned long value, char *buf, int radix)
{
	char tmp[33];
	int i = 0, j = 0;

	do {
		int d = value % radix;
		tmp[i++] = (d < 10) ? '0' + d : 'a' + d - 10;
		value /= radix;
	} while (value);

	while (i)
		buf[j++] = tmp[--i];
	buf[j] = 0;
	return buf;
}

char *ltoa(long value, char *buf, int radix)
{
	if (value < 0 && radix == 10) {
		buf[0] = '-';
		ultoa((unsigned long)-value, buf + 1, radix);
		return buf;
	}
	return ultoa((unsigned long)value, buf, radix);
}

char *itoa(int value, char *buf, int radix)
{
	return ltoa(value, buf, radix);
}

long filelength(int fd)
{
	struct stat st;

	if (fstat(fd, &st) < 0)
		return -1;
	return st.st_size;
}

void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext)
{
	const char *p, *slash, *dot;
	size_t n;

	if (drive) *drive = 0;
	if (dir) *dir = 0;
	if (fname) *fname = 0;
	if (ext) *ext = 0;

	p = strchr(path, ':');
	if (p) {
		if (drive) {
			n = p - path + 1;
			memcpy(drive, path, n);
			drive[n] = 0;
		}
		path = p + 1;
	}

	slash = NULL;
	for (p = path; *p; p++)
		if (*p == '/' || *p == '\\')
			slash = p;

	if (slash) {
		if (dir) {
			n = slash - path + 1;
			memcpy(dir, path, n);
			dir[n] = 0;
		}
		path = slash + 1;
	}

	dot = strrchr(path, '.');
	if (!dot)
		dot = path + strlen(path);

	if (fname) {
		n = dot - path;
		memcpy(fname, path, n);
		fname[n] = 0;
	}
	if (ext)
		strcpy(ext, dot);
}

void _makepath(char *path, const char *drive, const char *dir, const char *fname, const char *ext)
{
	size_t n;

	*path = 0;
	if (drive && *drive)
		strcat(path, drive);
	if (dir && *dir) {
		strcat(path, dir);
		n = strlen(path);
		if (path[n-1] != '/' && path[n-1] != '\\')
			strcat(path, "/");
	}
	if (fname)
		strcat(path, fname);
	if (ext && *ext) {
		if (*ext != '.')
			strcat(path, ".");
		strcat(path, ext);
	}
}

int getch(void)
{
	return 0;
}

int kbhit(void)
{
	return 0;
}

void delay(unsigned ms)
{
	sceKernelDelayThread(ms * 1000);
}

static int wild_match(const char *pat, const char *s)
{
	for (; *pat; pat++, s++) {
		if (*pat == '*') {
			while (*++pat == '*')
				;
			if (!*pat)
				return 1;
			for (; *s; s++)
				if (wild_match(pat, s))
					return 1;
			return 0;
		}
		if (!*s)
			return 0;
		if (*pat != '?' && tolower((unsigned char)*pat) != tolower((unsigned char)*s))
			return 0;
	}
	return *s == 0;
}

unsigned _dos_findnext(struct find_t *f)
{
	struct dirent *de;
	struct stat st;

	if (!f->handle)
		return 1;

	while ((de = readdir((DIR *)f->handle)) != NULL) {
		if (!wild_match(f->pattern, de->d_name))
			continue;
		strncpy(f->name, de->d_name, sizeof(f->name) - 1);
		f->name[sizeof(f->name) - 1] = 0;
		f->attrib = _A_NORMAL;
		f->size = 0;
		if (stat(de->d_name, &st) == 0) {
			f->size = st.st_size;
			if (S_ISDIR(st.st_mode))
				f->attrib = _A_SUBDIR;
		}
		return 0;
	}

	closedir((DIR *)f->handle);
	f->handle = NULL;
	return 1;
}

unsigned _dos_findfirst(const char *path, unsigned attr, struct find_t *f)
{
	const char *p, *pat;
	char dir[256];

	memset(f, 0, sizeof(*f));

	pat = path;
	for (p = path; *p; p++)
		if (*p == '/' || *p == '\\')
			pat = p + 1;

	if (pat == path)
		strcpy(dir, VITA_DATA_PATH);
	else {
		memcpy(dir, path, pat - path - 1);
		dir[pat - path - 1] = 0;
	}
	strncpy(f->pattern, pat, sizeof(f->pattern) - 1);

	f->handle = opendir(dir);
	return _dos_findnext(f);
}

unsigned _dos_findclose(struct find_t *f)
{
	if (f->handle)
		closedir((DIR *)f->handle);
	f->handle = NULL;
	return 0;
}

unsigned _dos_getdiskfree(unsigned drive, struct diskfree_t *df)
{
	df->total_clusters = 0xffff;
	df->avail_clusters = 0xffff;
	df->sectors_per_cluster = 64;
	df->bytes_per_sector = 512;
	return 0;
}

void _dos_getdrive(unsigned *drive)
{
	*drive = 3;
}

void _dos_setdrive(unsigned drive, unsigned *total)
{
	*total = 26;
}

// only used to bail out of deep recursion; the main thread stack is 1MB+
unsigned stackavail(void)
{
	return 256 * 1024;
}

#undef fopen

FILE *vita_fopen(const char *name, const char *mode)
{
	char path[256];
	char *base, *p;
	FILE *f;

	// DOS ate leading blanks, PLAYSAVE.C relies on that with "%8s.plr"
	while (*name == ' ')
		name++;

	f = fopen(name, mode);
	if (f || strlen(name) >= sizeof(path))
		return f;

	strcpy(path, name);
	base = strrchr(path, '/');
	base = base ? base + 1 : path;

	for (p = base; *p; p++)
		*p = tolower((unsigned char)*p);
	if ((f = fopen(path, mode)) != NULL)
		return f;

	for (p = base; *p; p++)
		*p = toupper((unsigned char)*p);
	return fopen(path, mode);
}
