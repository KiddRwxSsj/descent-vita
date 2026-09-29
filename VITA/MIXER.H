#ifndef _MIXER_H
#define _MIXER_H

#include "types.h"

#define MIX_VOICES	32

int mix_init(void);
void mix_close(void);
int mix_start(const ubyte *data, int length, int volume, int pan, int looping, int id);
void mix_stop(int handle);
int mix_done(int handle);
void mix_set_volume(int handle, int volume);
void mix_set_pan(int handle, int pan);
int mix_find_id(int id);
void mix_stop_all(void);

int music_init(const char *soundfont);
int music_start(const ubyte *data, int size, int loop);
void music_stop(void);
void music_set_volume(int volume);
void music_mix(int *out, int frames);

#endif
