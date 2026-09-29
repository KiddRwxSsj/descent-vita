#include <string.h>

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>

#include "types.h"
#include "mixer.h"

#define MIX_RATE		44100
#define MIX_GRAIN		512
#define SAMPLE_RATE		11025

// source position is 16.16, so one output frame advances a quarter sample
#define MIX_STEP		((SAMPLE_RATE << 16) / MIX_RATE)

typedef struct voice {
	const ubyte *data;
	int length;
	uint pos;
	int vol_l, vol_r;
	int volume, pan;
	int looping;
	int playing;
	int id;
	int serial;
} voice;

static voice voices[MIX_VOICES];
static int next_serial = 1;

static int audio_port = -1;
static SceUID mixer_thid = -1;
static volatile int mixer_running;
static short mix_buffer[2][MIX_GRAIN * 2];
static int mix_accum[MIX_GRAIN * 2];

static SceKernelLwMutexWork mix_lock __attribute__((aligned(8)));

static void lock(void)
{
	sceKernelLockLwMutex(&mix_lock, 1, NULL);
}

static void unlock(void)
{
	sceKernelUnlockLwMutex(&mix_lock, 1);
}

// SOS-style volume is 0..0x8000 per voice, pan 0..0xffff with 0x8000 centre
static void set_levels(voice *v, int volume, int pan)
{
	int l, r;

	if (volume < 0) volume = 0;
	if (volume > 0x8000) volume = 0x8000;
	if (pan < 0) pan = 0;
	if (pan > 0xffff) pan = 0xffff;

	v->volume = volume;
	v->pan = pan;

	l = ((0xffff - pan) * 2);
	r = pan * 2;
	if (l > 0xffff) l = 0xffff;
	if (r > 0xffff) r = 0xffff;

	v->vol_l = (volume * l) >> 16;
	v->vol_r = (volume * r) >> 16;
}

static void mix_voice(voice *v, int *out, int frames)
{
	int i, s0, s1, frac, s;
	uint idx;

	for (i = 0; i < frames; i++) {
		idx = v->pos >> 16;
		if (idx >= (uint)v->length) {
			if (!v->looping) {
				v->playing = 0;
				return;
			}
			v->pos -= (uint)v->length << 16;
			idx = v->pos >> 16;
		}

		s0 = (int)v->data[idx] - 128;
		s1 = (idx + 1 < (uint)v->length) ? (int)v->data[idx+1] - 128 : (v->looping ? (int)v->data[0] - 128 : s0);
		frac = (v->pos >> 8) & 0xff;
		s = (s0 << 8) + (s1 - s0) * frac;

		out[i*2] += (s * v->vol_l) >> 15;
		out[i*2+1] += (s * v->vol_r) >> 15;

		v->pos += MIX_STEP;
	}
}

static int mixer_main(SceSize args, void *argp)
{
	int buf = 0, i, s;

	while (mixer_running) {
		memset(mix_accum, 0, sizeof(mix_accum));

		lock();
		for (i = 0; i < MIX_VOICES; i++)
			if (voices[i].playing)
				mix_voice(&voices[i], mix_accum, MIX_GRAIN);
		unlock();

		music_mix(mix_accum, MIX_GRAIN);

		for (i = 0; i < MIX_GRAIN * 2; i++) {
			s = mix_accum[i];
			if (s > 32767) s = 32767;
			if (s < -32768) s = -32768;
			mix_buffer[buf][i] = s;
		}

		sceAudioOutOutput(audio_port, mix_buffer[buf]);
		buf ^= 1;
	}

	return sceKernelExitDeleteThread(0);
}

int mix_init(void)
{
	int vol[2] = { SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB };

	if (audio_port >= 0)
		return 1;

	audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, MIX_GRAIN, MIX_RATE, SCE_AUDIO_OUT_MODE_STEREO);
	if (audio_port < 0)
		return 0;

	sceAudioOutSetVolume(audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);
	sceKernelCreateLwMutex(&mix_lock, "mix_lock", 0, 0, NULL);
	memset(voices, 0, sizeof(voices));

	mixer_running = 1;
	mixer_thid = sceKernelCreateThread("mixer", mixer_main, 0x10000100 - 10, 0x4000, 0, SCE_KERNEL_CPU_MASK_USER_2, NULL);
	sceKernelStartThread(mixer_thid, 0, NULL);
	return 1;
}

void mix_close(void)
{
	if (audio_port < 0)
		return;
	mixer_running = 0;
	sceKernelWaitThreadEnd(mixer_thid, NULL, NULL);
	sceAudioOutReleasePort(audio_port);
	audio_port = -1;
}

// handles carry a serial in the high bits so a stale handle can never
// touch a voice that has since been reused
static voice *find(int handle)
{
	voice *v;

	if (handle < 0)
		return NULL;
	v = &voices[handle & (MIX_VOICES-1)];
	if (v->serial != (handle >> 8) || !v->playing)
		return NULL;
	return v;
}

int mix_start(const ubyte *data, int length, int volume, int pan, int looping, int id)
{
	int i, best = -1;
	voice *v;

	if (audio_port < 0 || !data || length <= 0)
		return -1;

	lock();
	for (i = 0; i < MIX_VOICES; i++)
		if (!voices[i].playing) {
			best = i;
			break;
		}
	if (best < 0) {
		unlock();
		return -1;
	}

	v = &voices[best];
	v->data = data;
	v->length = length;
	v->pos = 0;
	v->looping = looping;
	v->id = id;
	v->serial = next_serial++ & 0xffffff;
	set_levels(v, volume, pan);
	v->playing = 1;
	unlock();

	return (v->serial << 8) | best;
}

void mix_stop(int handle)
{
	voice *v;

	lock();
	if ((v = find(handle)) != NULL)
		v->playing = 0;
	unlock();
}

int mix_done(int handle)
{
	int done;

	lock();
	done = (find(handle) == NULL);
	unlock();
	return done;
}

void mix_set_volume(int handle, int volume)
{
	voice *v;

	lock();
	if ((v = find(handle)) != NULL)
		set_levels(v, volume, v->pan);
	unlock();
}

void mix_set_pan(int handle, int pan)
{
	voice *v;

	lock();
	if ((v = find(handle)) != NULL)
		set_levels(v, v->volume, pan);
	unlock();
}

int mix_find_id(int id)
{
	int i, h = -1;

	lock();
	for (i = 0; i < MIX_VOICES; i++)
		if (voices[i].playing && voices[i].id == id) {
			h = (voices[i].serial << 8) | i;
			break;
		}
	unlock();
	return h;
}

void mix_stop_all(void)
{
	int i;

	lock();
	for (i = 0; i < MIX_VOICES; i++)
		voices[i].playing = 0;
	unlock();
}
