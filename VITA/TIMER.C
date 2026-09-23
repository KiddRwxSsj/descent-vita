#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>

#include "fix.h"
#include "timer.h"

static SceUInt64 timer_base;
static int timer_installed;
static void (*timer_function)();

int timer_paused = 0;

int vita_fps_cap = 60;

static SceUInt64 usecs(void)
{
	if (!timer_installed) {
		timer_base = sceKernelGetProcessTimeWide();
		timer_installed = 1;
	}
	return sceKernelGetProcessTimeWide() - timer_base;
}

void timer_init()
{
	usecs();
}

void timer_close()
{
}

void timer_set_rate(int count_val)
{
}

void timer_set_function( void _far * function )
{
	timer_function = function;
}

void timer_set_joyhandler( void (*joy_handler)() )
{
}

fix timer_get_fixed_seconds()
{
	return (fix)((usecs() * 65536) / 1000000);
}

fix timer_get_fixed_secondsX()
{
	return timer_get_fixed_seconds();
}

fix timer_get_approx_seconds()
{
	return timer_get_fixed_seconds();
}

unsigned int timer_get_microseconds()
{
	return (unsigned int)usecs();
}

unsigned int timer_get_milliseconds()
{
	return (unsigned int)(usecs() / 1000);
}

// BIOS tick count, the PIT ran it at 1193180/65536 Hz
int timer_get_ticker()
{
	return (int)((usecs() * 1193180ULL) / (65536ULL * 1000000ULL));
}

void vita_delay_us(int us)
{
	if (us > 0)
		sceKernelDelayThread(us);
}
