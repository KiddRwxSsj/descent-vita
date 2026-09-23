#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#include "types.h"
#include "fix.h"
#include "joy.h"
#include "vita.h"

#define MAX_BUTTONS	20
#define PIT_HZ		1193180

typedef struct Button_info {
	ubyte		ignore;
	ubyte		state;
	ubyte		last_state;
	int		timedown;
	ubyte		downcount;
	ubyte		upcount;
} Button_info;

typedef struct Joy_info {
	ubyte			present_mask;
	ubyte			slow_read;
	int			max_timer;
	Button_info	buttons[MAX_BUTTONS];
	int			axis_min[4];
	int			axis_center[4];
	int			axis_max[4];
} Joy_info;

static Joy_info joystick;
static volatile unsigned char raw_axes[4] = { 128, 128, 128, 128 };
static SceUInt64 last_update;
static SceKernelLwMutexWork joy_lock __attribute__((aligned(8)));
static int lock_ready;

char joy_installed = 0;

int vita_stick_invert = 0;
char joy_present = 0;

// pad bit for each joystick button number
static const unsigned int joy_button_bits[MAX_BUTTONS] = {
	SCE_CTRL_CROSS, SCE_CTRL_CIRCLE, SCE_CTRL_SQUARE, SCE_CTRL_TRIANGLE,
	SCE_CTRL_LTRIGGER | SCE_CTRL_L1, SCE_CTRL_RTRIGGER | SCE_CTRL_R1,
	SCE_CTRL_UP, SCE_CTRL_DOWN, SCE_CTRL_LEFT, SCE_CTRL_RIGHT,
	SCE_CTRL_L3, SCE_CTRL_R3,
};

static void joy_lock_on(void)
{
	if (lock_ready)
		sceKernelLockLwMutex(&joy_lock, 1, NULL);
}

static void joy_lock_off(void)
{
	if (lock_ready)
		sceKernelUnlockLwMutex(&joy_lock, 1);
}

void vita_joy_update(unsigned int buttons, const unsigned char *axes)
{
	SceUInt64 now = sceKernelGetProcessTimeWide();
	int ticks, i, state;
	Button_info *b;

	ticks = last_update ? (int)(((now - last_update) * PIT_HZ) / 1000000) : 0;
	last_update = now;

	for (i = 0; i < 4; i++)
		raw_axes[i] = axes[i];

	if (!joy_installed)
		return;

	joy_lock_on();
	for (i = 0; i < MAX_BUTTONS; i++) {
		b = &joystick.buttons[i];
		if (b->ignore)
			continue;
		state = joy_button_bits[i] ? ((buttons & joy_button_bits[i]) != 0) : 0;
		if (b->last_state == state) {
			if (state) b->timedown += ticks;
		} else {
			if (state) {
				b->downcount++;
				b->state = 1;
			} else {
				b->upcount += b->state;
				b->state = 0;
			}
			b->last_state = state;
		}
	}
	joy_lock_off();
}

void joy_get_cal_vals(int *axis_min, int *axis_center, int *axis_max)
{
	int i;

	for (i=0; i<4; i++)		{
		axis_min[i] = joystick.axis_min[i];
		axis_center[i] = joystick.axis_center[i];
		axis_max[i] = joystick.axis_max[i];
	}
}

// the sticks have a fixed range, so stored calibration from an old config
// is accepted but never allowed to override it
void joy_set_cal_vals(int *axis_min, int *axis_center, int *axis_max)
{
	int i;

	for (i=0; i<4; i++)		{
		joystick.axis_min[i] = 0;
		joystick.axis_center[i] = 128;
		joystick.axis_max[i] = 255;
	}
}

ubyte joy_get_present_mask()
{
	return joystick.present_mask;
}

void joy_set_timer_rate(int max_value )
{
	joystick.max_timer = max_value;
}

int joy_get_timer_rate()
{
	return joystick.max_timer;
}

void joy_flush()
{
	int i;

	if (!joy_installed) return;

	joy_lock_on();
	for (i=0; i<MAX_BUTTONS; i++ )	{
		joystick.buttons[i].ignore = 0;
		joystick.buttons[i].state = 0;
		joystick.buttons[i].timedown = 0;
		joystick.buttons[i].downcount = 0;
		joystick.buttons[i].upcount = 0;
	}
	joy_lock_off();
}

ubyte joy_read_raw_buttons()
{
	int i, v = 0;

	for (i = 0; i < 4; i++)
		if (joystick.buttons[i].state)
			v |= 1 << i;
	return v;
}

void joy_set_slow_reading(int flag)
{
	joystick.slow_read |= flag;
}

ubyte joystick_read_raw_axis( ubyte mask, int * axis )
{
	int i;
	ubyte read_masks = 0;

	for (i = 0; i < 4; i++) {
		axis[i] = 0;
		if (joy_installed && (mask & joystick.present_mask & (1 << i))) {
			axis[i] = raw_axes[i];
			read_masks |= 1 << i;
		}
	}
	return read_masks;
}

int joy_init()
{
	int i;

	if (!lock_ready) {
		sceKernelCreateLwMutex(&joy_lock, "joy_lock", 0, 0, NULL);
		lock_ready = 1;
	}

	joy_installed = 1;
	joy_flush();

	for (i=0; i<MAX_BUTTONS; i++ )
		joystick.buttons[i].last_state = 0;

	joy_set_cal_vals(NULL, NULL, NULL);
	joystick.present_mask = JOY_ALL_AXIS;
	joy_present = 1;

	return joy_present;
}

void joy_close()
{
	joy_installed = 0;
}

void joy_set_ul()
{
}

void joy_set_lr()
{
}

void joy_set_cen()
{
}

void joy_set_cen_fake(int channel)
{
}

int joy_get_scaled_reading( int raw, int axn )
{
	int x, d;

	raw -= joystick.axis_center[axn];
	if ( raw < 0 )
		d = joystick.axis_center[axn]-joystick.axis_min[axn];
	else
		d = joystick.axis_max[axn]-joystick.axis_center[axn];

	x = d ? (raw << 7) / d : 0;

	if ( x < -128 ) x = -128;
	if ( x > 127 ) x = 127;

	return x;
}

void joy_get_pos( int *x, int *y )
{
	if ((!joy_installed)||(!joy_present)) { *x=*y=0; return; }

	*x = joy_get_scaled_reading( raw_axes[0], 0 );
	*y = joy_get_scaled_reading( raw_axes[1], 1 );
}

ubyte joy_read_stick( ubyte masks, int *axis )
{
	int i;
	ubyte flags = joystick_read_raw_axis( masks, axis );

	for (i = 0; i < 4; i++)
		axis[i] = (flags & (1 << i)) ? joy_get_scaled_reading( axis[i], i ) : 0;
	return flags;
}

int joy_get_btns()
{
	if ((!joy_installed)||(!joy_present)) return 0;
	return joy_read_raw_buttons();
}

void joy_get_btn_down_cnt( int *btn0, int *btn1 )
{
	if ((!joy_installed)||(!joy_present)) { *btn0=*btn1=0; return; }

	joy_lock_on();
	*btn0 = joystick.buttons[0].downcount;
	joystick.buttons[0].downcount = 0;
	*btn1 = joystick.buttons[1].downcount;
	joystick.buttons[1].downcount = 0;
	joy_lock_off();
}

int joy_get_button_state( int btn )
{
	if ((!joy_installed)||(!joy_present)) return 0;
	if ( btn >= MAX_BUTTONS ) return 0;

	return joystick.buttons[btn].state;
}

int joy_get_button_up_cnt( int btn )
{
	int count;

	if ((!joy_installed)||(!joy_present)) return 0;
	if ( btn >= MAX_BUTTONS ) return 0;

	joy_lock_on();
	count = joystick.buttons[btn].upcount;
	joystick.buttons[btn].upcount = 0;
	joy_lock_off();

	return count;
}

int joy_get_button_down_cnt( int btn )
{
	int count;

	if ((!joy_installed)||(!joy_present)) return 0;
	if ( btn >= MAX_BUTTONS ) return 0;

	joy_lock_on();
	count = joystick.buttons[btn].downcount;
	joystick.buttons[btn].downcount = 0;
	joy_lock_off();

	return count;
}

fix joy_get_button_down_time( int btn )
{
	fix count;

	if ((!joy_installed)||(!joy_present)) return 0;
	if ( btn >= MAX_BUTTONS ) return 0;

	joy_lock_on();
	count = joystick.buttons[btn].timedown;
	joystick.buttons[btn].timedown = 0;
	joy_lock_off();

	return fixmuldiv(count, 65536, PIT_HZ);
}

void joy_get_btn_up_cnt( int *btn0, int *btn1 )
{
	if ((!joy_installed)||(!joy_present)) { *btn0=*btn1=0; return; }

	joy_lock_on();
	*btn0 = joystick.buttons[0].upcount;
	joystick.buttons[0].upcount = 0;
	*btn1 = joystick.buttons[1].upcount;
	joystick.buttons[1].upcount = 0;
	joy_lock_off();
}

void joy_set_btn_values( int btn, int state, fix timedown, int downcount, int upcount )
{
	joy_lock_on();
	joystick.buttons[btn].ignore = 1;
	joystick.buttons[btn].state = state;
	joystick.buttons[btn].timedown = fixmuldiv( timedown, PIT_HZ, 65536 );
	joystick.buttons[btn].downcount = downcount;
	joystick.buttons[btn].upcount = upcount;
	joy_lock_off();
}

void joy_poll()
{
}
