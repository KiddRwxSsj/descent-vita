#include <stdlib.h>
#include <string.h>

#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>

#include "error.h"
#include "key.h"
#include "timer.h"
#include "vita.h"

#define KEY_BUFFER_SIZE 16

char 							keyd_buffer_type;
char 							keyd_repeat;
char 							keyd_editor_mode;
volatile unsigned char 	keyd_last_pressed;
volatile unsigned char 	keyd_last_released;
volatile unsigned char	keyd_pressed[256];
volatile int				keyd_time_when_last_pressed;

typedef struct keyboard	{
	unsigned short		keybuffer[KEY_BUFFER_SIZE];
	fix					time_pressed[KEY_BUFFER_SIZE];
	fix					TimeKeyWentDown[256];
	fix					TimeKeyHeldDown[256];
	unsigned int		NumDowns[256];
	unsigned int		NumUps[256];
	unsigned int 		keyhead, keytail;
} keyboard;

static volatile keyboard key_data;

static unsigned char Installed=0;

// stands in for the cli/sti pairs around the shared key state
static SceKernelLwMutexWork key_lock __attribute__((aligned(8)));

unsigned char ascii_table[128] =
{ 255, 255, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',255,255,
  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 255, 255,
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', 39, '`',
  255, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 255,'*',
  255, ' ', 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,255,255,
  255, 255, 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255 };

unsigned char shifted_ascii_table[128] =
{ 255, 255, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',255,255,
  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 255, 255,
  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
  255, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 255,255,
  255, ' ', 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,255,255,
  255, 255, 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
  255,255,255,255,255,255,255,255 };

static int in_game(SceUInt64 now);

static void key_lock_on(void)
{
	sceKernelLockLwMutex(&key_lock, 1, NULL);
}

static void key_lock_off(void)
{
	sceKernelUnlockLwMutex(&key_lock, 1);
}

char key_to_ascii(int keycode )
{
	int shifted;

	shifted = keycode & KEY_SHIFTED;
	keycode &= 0xFF;

	if ( keycode>=127 )
		return 255;

	if (shifted)
		return shifted_ascii_table[keycode];
	else
		return ascii_table[keycode];
}

void key_flush()
{
	int i;
	fix CurTime;

	key_lock_on();

	key_data.keyhead = key_data.keytail = 0;

	for (i=0; i<KEY_BUFFER_SIZE; i++ )	{
		key_data.keybuffer[i] = 0;
		key_data.time_pressed[i] = 0;
	}

	CurTime = timer_get_fixed_secondsX();

	for (i=0; i<256; i++ )	{
		keyd_pressed[i] = 0;
		key_data.TimeKeyWentDown[i] = CurTime;
		key_data.TimeKeyHeldDown[i] = 0;
		key_data.NumDowns[i]=0;
		key_data.NumUps[i]=0;
	}

	key_lock_off();
}

static int add_one( int n )
{
	n++;
	if ( n >= KEY_BUFFER_SIZE ) n=0;
	return n;
}

int key_checkch()
{
	int is_one_waiting = 0;

	key_lock_on();
	if (key_data.keytail!=key_data.keyhead)
		is_one_waiting = 1;
	key_lock_off();

	if (!is_one_waiting)
		sceKernelDelayThread(1000);

	return is_one_waiting;
}

int key_inkey()
{
	int key = 0;

	key_lock_on();
	if (key_data.keytail!=key_data.keyhead)	{
		key = key_data.keybuffer[key_data.keyhead];
		key_data.keyhead = add_one(key_data.keyhead);
	}
	key_lock_off();

	// menus poll this flat out, a nap between polls takes the core from 100% to idle
	if (!key && !in_game(sceKernelGetProcessTimeWide()))
		sceKernelDelayThread(1000);

	return key;
}

int key_inkey_time(fix * time)
{
	int key = 0;

	key_lock_on();
	if (key_data.keytail!=key_data.keyhead)	{
		key = key_data.keybuffer[key_data.keyhead];
		*time = key_data.time_pressed[key_data.keyhead];
		key_data.keyhead = add_one(key_data.keyhead);
	}
	key_lock_off();

	return key;
}

int key_peekkey()
{
	int key = 0;

	key_lock_on();
	if (key_data.keytail!=key_data.keyhead)
		key = key_data.keybuffer[key_data.keyhead];
	key_lock_off();

	return key;
}

int key_getch()
{
	while (!key_checkch())
		;
	return key_inkey();
}

unsigned int key_get_shift_status()
{
	unsigned int shift_status = 0;

	if ( keyd_pressed[KEY_LSHIFT] || keyd_pressed[KEY_RSHIFT] )
		shift_status |= KEY_SHIFTED;
	if ( keyd_pressed[KEY_LALT] || keyd_pressed[KEY_RALT] )
		shift_status |= KEY_ALTED;
	if ( keyd_pressed[KEY_LCTRL] || keyd_pressed[KEY_RCTRL] )
		shift_status |= KEY_CTRLED;

	return shift_status;
}

fix key_down_time(int scancode)
{
	fix time_down, time;

	if ((scancode<0)|| (scancode>255)) return 0;

	key_lock_on();
	if ( !keyd_pressed[scancode] )	{
		time_down = key_data.TimeKeyHeldDown[scancode];
		key_data.TimeKeyHeldDown[scancode] = 0;
	} else	{
		time = timer_get_fixed_secondsX();
		time_down =  time - key_data.TimeKeyWentDown[scancode];
		key_data.TimeKeyWentDown[scancode] = time;
	}
	key_lock_off();

	return time_down;
}

unsigned int key_down_count(int scancode)
{
	int n;

	if ((scancode<0)|| (scancode>255)) return 0;

	key_lock_on();
	n = key_data.NumDowns[scancode];
	key_data.NumDowns[scancode] = 0;
	key_lock_off();

	return n;
}

unsigned int key_up_count(int scancode)
{
	int n;

	if ((scancode<0)|| (scancode>255)) return 0;

	key_lock_on();
	n = key_data.NumUps[scancode];
	key_data.NumUps[scancode] = 0;
	key_lock_off();

	return n;
}

// what the int 9 handler did with each make/break code
static void key_handler(unsigned char scancode, int breakbit)
{
	unsigned short keycode;
	unsigned int temp;

	key_lock_on();

	if (breakbit)	{
		keyd_last_released = scancode;
		keyd_pressed[scancode] = 0;
		key_data.NumUps[scancode]++;
		key_data.TimeKeyHeldDown[scancode] += timer_get_fixed_secondsX() - key_data.TimeKeyWentDown[scancode];
	} else {
		keyd_last_pressed = scancode;
		keyd_time_when_last_pressed = timer_get_fixed_secondsX();
		if (!keyd_pressed[scancode])	{
			key_data.TimeKeyWentDown[scancode] = timer_get_fixed_secondsX();
			keyd_pressed[scancode] = 1;
			key_data.NumDowns[scancode]++;
		} else if (!keyd_repeat) {
			scancode = 0xAA;
		}

		if ( scancode!=0xAA ) {
			keycode = scancode;

			if ( keyd_pressed[KEY_LSHIFT] || keyd_pressed[KEY_RSHIFT] )
				keycode |= KEY_SHIFTED;
			if ( keyd_pressed[KEY_LALT] || keyd_pressed[KEY_RALT] )
				keycode |= KEY_ALTED;
			if ( keyd_pressed[KEY_LCTRL] || keyd_pressed[KEY_RCTRL] )
				keycode |= KEY_CTRLED;

			temp = key_data.keytail+1;
			if ( temp >= KEY_BUFFER_SIZE ) temp=0;

			if (temp!=key_data.keyhead)	{
				key_data.keybuffer[key_data.keytail] = keycode;
				key_data.time_pressed[key_data.keytail] = keyd_time_when_last_pressed;
				key_data.keytail = temp;
			}
		}
	}

	key_lock_off();
}

void key_init()
{
	if (!Installed)
		sceKernelCreateLwMutex(&key_lock, "key_lock", 0, 0, NULL);

	keyd_time_when_last_pressed = timer_get_fixed_seconds();
	keyd_buffer_type = 1;
	keyd_repeat = 1;

	key_flush();

	if (Installed) return;
	Installed = 1;

	vita_input_init();

	atexit( key_close );
}

void key_close()
{
	if (!Installed) return;
	Installed = 0;
}

void key_debug()
{
}

//------------------------------------------------------------------------
// Pad to keyboard translation. While the game loop is reading controls
// the buttons drive flight keys, everywhere else they drive the menus.

#define GAME_TIMEOUT_US	150000
#define REPEAT_DELAY_US	400000
#define REPEAT_RATE_US	80000

// the rear panel halves ride along as two extra buttons on bits sceCtrl never sets
#define REAR_TOUCH_LEFT		0x01000000
#define REAR_TOUCH_RIGHT	0x02000000

// pad order is the order the button mapping menu shows them in. Start sits
// past the end: it's always Esc so nobody can map their way out of the menus
#define START_SLOT		VITA_PAD_BUTTONS
#define NUM_PAD_BITS	(VITA_PAD_BUTTONS + 1)

static const unsigned int pad_bits[NUM_PAD_BITS] = {
	SCE_CTRL_UP, SCE_CTRL_DOWN, SCE_CTRL_LEFT, SCE_CTRL_RIGHT,
	SCE_CTRL_CROSS, SCE_CTRL_CIRCLE, SCE_CTRL_SQUARE, SCE_CTRL_TRIANGLE,
	SCE_CTRL_LTRIGGER | SCE_CTRL_L1, SCE_CTRL_RTRIGGER | SCE_CTRL_R1, SCE_CTRL_SELECT,
	REAR_TOUCH_LEFT, REAR_TOUCH_RIGHT,
	SCE_CTRL_START,
};

const char *vita_button_names[VITA_PAD_BUTTONS] = {
	"Up", "Down", "Left", "Right",
	"Cross", "Circle", "Square", "Triangle",
	"L", "R", "Select",
	"Rear touch left", "Rear touch right",
};

static const unsigned char menu_keys[NUM_PAD_BITS] = {
	KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
	KEY_ENTER, KEY_ESC, KEY_SPACEBAR, KEY_BACKSP,
	KEY_PAGEUP, KEY_PAGEDOWN, KEY_TAB,
	0, 0,
	KEY_ESC,
};

// keys are the stock keyboard bindings, the keyboard config is pinned to defaults
typedef struct pad_action {
	const char *name;
	unsigned char key;
} pad_action;

static const pad_action actions[] = {
	{ "Nothing",			0 },
	{ "Fire primary",		KEY_LCTRL },
	{ "Fire secondary",	KEY_SPACEBAR },
	{ "Next primary",		KEY_VITA_NEXT_PRIMARY },
	{ "Next secondary",	KEY_VITA_NEXT_SECONDARY },
	{ "Flare",				KEY_F },
	{ "Drop bomb",			KEY_B },
	{ "Accelerate",		KEY_A },
	{ "Reverse",			KEY_Z },
	{ "Slide left",		KEY_PAD1 },
	{ "Slide right",		KEY_PAD3 },
	{ "Slide up",			KEY_PADMINUS },
	{ "Slide down",		KEY_PADPLUS },
	{ "Bank left",			KEY_Q },
	{ "Bank right",		KEY_E },
	{ "Rear view",			KEY_R },
	{ "Automap",			KEY_TAB },
	{ "Cockpit view",		KEY_F3 },
	{ "Options",			KEY_F2 },
};

int vita_num_actions = sizeof(actions) / sizeof(actions[0]);

static const int default_map[VITA_PAD_BUTTONS] = {
	11, 12, 13, 14,
	3, 6, 5, 4,
	2, 1, 16,
	15, 17,
};

int vita_button_map[VITA_PAD_BUTTONS];

const char *vita_action_name(int action)
{
	if (action < 0 || action >= vita_num_actions)
		action = 0;
	return actions[action].name;
}

void vita_reset_buttons(void)
{
	memcpy(vita_button_map, default_map, sizeof(default_map));
}

static unsigned char held_key[NUM_PAD_BITS];
static SceUInt64 repeat_time[NUM_PAD_BITS];
static volatile SceUInt64 last_game_frame;

static SceUID input_thid = -1;

extern void vita_joy_update(unsigned int buttons, const unsigned char *axes);

void vita_controls_frame(void)
{
	last_game_frame = sceKernelGetProcessTimeWide();
}

static int in_game(SceUInt64 now)
{
	return last_game_frame && (now - last_game_frame) < GAME_TIMEOUT_US;
}

static void pad_update(unsigned int buttons, SceUInt64 now)
{
	int i, game = in_game(now);
	unsigned char k;

	for (i = 0; i < NUM_PAD_BITS; i++) {
		int down = (buttons & pad_bits[i]) != 0;

		if (down && !held_key[i]) {
			if (!game || i == START_SLOT)
				k = menu_keys[i];
			else
				k = actions[vita_button_map[i]].key;
			if (!k)
				continue;
			held_key[i] = k;
			repeat_time[i] = now + REPEAT_DELAY_US;
			key_handler(k, 0);
		} else if (!down && held_key[i]) {
			key_handler(held_key[i], 1);
			held_key[i] = 0;
		} else if (down && !game && now >= repeat_time[i]) {
			// typematic repeat so held d-pad scrolls menus like a real keyboard
			repeat_time[i] = now + REPEAT_RATE_US;
			key_handler(held_key[i], 0);
		}
	}
}

static int input_main(SceSize args, void *argp)
{
	SceCtrlData pad;
	SceTouchData touch;
	unsigned char axes[4];
	int i;

	for (;;) {
		memset(&pad, 0, sizeof(pad));
		if (sceCtrlPeekBufferPositive(0, &pad, 1) < 0)
			pad.lx = pad.ly = pad.rx = pad.ry = 128;

		// rear panel is 1920 wide, split down the middle
		memset(&touch, 0, sizeof(touch));
		sceTouchPeek(SCE_TOUCH_PORT_BACK, &touch, 1);
		for (i = 0; i < touch.reportNum; i++)
			pad.buttons |= (touch.report[i].x < 960) ? REAR_TOUCH_LEFT : REAR_TOUCH_RIGHT;

		axes[0] = pad.lx;
		axes[1] = pad.ly;
		axes[2] = pad.rx;
		axes[3] = pad.ry;

		pad_update(pad.buttons, sceKernelGetProcessTimeWide());
		vita_joy_update(pad.buttons, axes);

		sceKernelDelayThread(4000);
	}
	return 0;
}

void vita_input_init(void)
{
	if (input_thid >= 0)
		return;

	sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG_WIDE);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_BACK, SCE_TOUCH_SAMPLING_STATE_START);

	input_thid = sceKernelCreateThread("input", input_main, 0x10000100 - 8, 0x4000, 0, SCE_KERNEL_CPU_MASK_USER_2, NULL);
	sceKernelStartThread(input_thid, 0, NULL);
}
