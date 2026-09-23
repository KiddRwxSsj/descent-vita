#ifndef _VITA_H
#define _VITA_H

#define VITA_DATA_PATH	"ux0:data/descent"

#define VITA_SCREEN_4_3		0
#define VITA_SCREEN_STRETCH	1

extern int vita_screen_mode;
extern int vita_smooth;
extern int vita_fps_cap;

#define VITA_PAD_BUTTONS	13

// bits 0-3: left X, left Y, right X, right Y flipped from the pushed direction
extern int vita_stick_invert;
extern int vita_button_map[VITA_PAD_BUTTONS];
extern const char *vita_button_names[VITA_PAD_BUTTONS];
extern int vita_num_actions;
const char *vita_action_name(int action);
void vita_reset_buttons(void);

void vita_video_init(void);
void vita_video_shutdown(void);
void vita_video_lock(void);
void vita_video_unlock(void);
void vita_dac_out(unsigned short port, unsigned char value);
void vita_show_canvas(struct _grs_canvas *canv);
unsigned char vita_dac_in(unsigned short port);

void vita_input_init(void);
void vita_input_poll(void);

int vita_joy_axis(int axis);
int vita_joy_buttons(void);

void vita_sound_init(void);
void vita_sound_shutdown(void);

void vita_fatal(const char *msg);
void vita_delay_us(int us);

int vita_ime_input(const char *title, char *text, int max_len);

#endif
