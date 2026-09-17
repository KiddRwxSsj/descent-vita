#ifndef _VITA_PRELUDE_H
#define _VITA_PRELUDE_H

// Pull every libc header in before any game header gets a chance to mess
// with packing, so newlib's own structs keep the layout libc was built with.
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <setjmp.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

// DOS segment/ISR qualifiers mean nothing in a flat 32-bit ARM address space
#define far
#define _far
#define __far
#define near
#define _near
#define __near
#define huge
#define _interrupt
#define __interrupt
#define interrupt

#define __loadds
#define _loadds
#define _cdecl
#define __cdecl

// DOS filenames were case-blind; the Vita is too, Vita3K on Linux is not
FILE *vita_fopen(const char *name, const char *mode);
#define fopen		vita_fopen

// Watcom's rand() was 15 bits and the game leans on that everywhere (rand()*n>>15)
#undef RAND_MAX
#define RAND_MAX	0x7fff
#define rand()		(rand() & RAND_MAX)

#define stricmp		strcasecmp
#define strcmpi		strcasecmp

#define strnicmp	strncasecmp
#define _MAX_PATH	260
#define _MAX_DRIVE	3
#define _MAX_DIR	256
#define _MAX_FNAME	256
#define _MAX_EXT	256

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

char *strupr(char *s);
char *strlwr(char *s);
char *strrev(char *s);
char *itoa(int value, char *buf, int radix);
char *ltoa(long value, char *buf, int radix);
char *ultoa(unsigned long value, char *buf, int radix);
long filelength(int fd);
void _splitpath(const char *path, char *drive, char *dir, char *fname, char *ext);
void _makepath(char *path, const char *drive, const char *dir, const char *fname, const char *ext);
int getch(void);
int kbhit(void);
void delay(unsigned ms);
unsigned stackavail(void);

#endif
