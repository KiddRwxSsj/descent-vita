#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/io/stat.h>
#include <psp2/power.h>

#include "vita.h"

#define MAX_ARGS	64

// the soundfont alone is ~64MB once TinySoundFont turns its samples into floats
unsigned int _newlib_heap_size_user = 128 * 1024 * 1024;

extern int descent_main(int argc, char **argv);

static char *args[MAX_ARGS];
static char args_text[1024];

// args.txt in the data folder stands in for the DOS command line
static int read_args_file(int n)
{
	FILE *f;
	size_t len;
	char *p;

	f = fopen("args.txt", "r");
	if (!f)
		return n;

	len = fread(args_text, 1, sizeof(args_text) - 1, f);
	fclose(f);
	args_text[len] = 0;

	p = args_text;
	while (*p && n < MAX_ARGS - 1) {
		while (*p && isspace((unsigned char)*p))
			p++;
		if (!*p)
			break;
		args[n++] = p;
		while (*p && !isspace((unsigned char)*p))
			p++;
		if (*p)
			*p++ = 0;
	}
	return n;
}

void vita_fatal(const char *msg)
{
	FILE *f = fopen(VITA_DATA_PATH "/error.log", "w");

	if (f) {
		fputs(msg, f);
		fputc('\n', f);
		fclose(f);
	}
}

int main(int argc, char **argv)
{
	int n = 0;

	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);

	// the DOS code busy-polls everywhere; keep it off the scanout/input/mixer cores
	sceKernelChangeThreadCpuAffinityMask(sceKernelGetThreadId(), SCE_KERNEL_CPU_MASK_USER_0);

	sceIoMkdir(VITA_DATA_PATH, 0777);
	chdir(VITA_DATA_PATH);

	args[n++] = "descent";
	args[n++] = "-nodoscheck";
	args[n++] = "-nofilecheck";
	args[n++] = "-nomemcheck";
	n = read_args_file(n);
	args[n] = NULL;

	return descent_main(n, args);
}
