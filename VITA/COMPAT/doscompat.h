#ifndef _DOSCOMPAT_H
#define _DOSCOMPAT_H

#define EZERO	0

struct DWORDREGS {
	unsigned int eax, ebx, ecx, edx, esi, edi, cflag;
};

struct WORDREGS {
	unsigned short ax, _1, bx, _2, cx, _3, dx, _4, si, _5, di, _6;
	unsigned int cflag;
};

struct BYTEREGS {
	unsigned char al, ah, _1, _2, bl, bh, _3, _4, cl, ch, _5, _6, dl, dh, _7, _8;
};

union REGS {
	struct DWORDREGS x;
	struct WORDREGS w;
	struct BYTEREGS h;
};

struct SREGS {
	unsigned short es, cs, ss, ds, fs, gs;
};

int int386(int intno, union REGS *in, union REGS *out);
int int386x(int intno, union REGS *in, union REGS *out, struct SREGS *seg);
void segread(struct SREGS *seg);

#define _A_NORMAL	0x00
#define _A_RDONLY	0x01
#define _A_HIDDEN	0x02
#define _A_SYSTEM	0x04
#define _A_VOLID	0x08
#define _A_SUBDIR	0x10
#define _A_ARCH		0x20

struct find_t {
	void *handle;
	char pattern[64];
	char attrib;
	unsigned short wr_time;
	unsigned short wr_date;
	unsigned long size;
	char name[256];
};

unsigned _dos_findfirst(const char *path, unsigned attr, struct find_t *f);
unsigned _dos_findnext(struct find_t *f);
unsigned _dos_findclose(struct find_t *f);

struct diskfree_t {
	unsigned total_clusters;
	unsigned avail_clusters;
	unsigned sectors_per_cluster;
	unsigned bytes_per_sector;
};

unsigned _dos_getdiskfree(unsigned drive, struct diskfree_t *df);
void _dos_getdrive(unsigned *drive);
void _dos_setdrive(unsigned drive, unsigned *total);

#define _HARDERR_IGNORE	0
#define _HARDERR_RETRY	1
#define _HARDERR_ABORT	2
#define _HARDERR_FAIL	3

void _harderr(void *handler);

unsigned char inp(unsigned short port);
unsigned short inpw(unsigned short port);
unsigned char outp(unsigned short port, unsigned char value);
unsigned short outpw(unsigned short port, unsigned short value);

void _disable(void);
void _enable(void);

#endif
