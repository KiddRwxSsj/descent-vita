#include <stdlib.h>
#include <string.h>

#include <psp2/display.h>
#include <psp2/kernel/threadmgr.h>
#include <vita2d.h>

#include "gr.h"
#include "grdef.h"
#include "palette.h"
#include "vita.h"

#define DISPLAY_W		960
#define DISPLAY_H		544

#define MAX_SCREEN_W	640
#define MAX_SCREEN_H	480

int gr_installed = 0;
unsigned char *gr_video_memory;
char gr_pal_default[768];
unsigned short _A0000;

int vita_screen_mode = VITA_SCREEN_4_3;
int vita_smooth = 0;

static ubyte screen_buffer[MAX_SCREEN_W * MAX_SCREEN_H];

// first byte on screen; the two-page modes (automap) flip it like the VGA start address
static ubyte *show_start = screen_buffer;

static SceUID scanout_thid = -1;
static volatile int scanout_running;

// 8-byte alignment is a hard requirement of the lw mutex work area
static SceKernelLwMutexWork screen_lock __attribute__((aligned(8)));

static ubyte dac[768];
static volatile int dac_dirty = 1;
static int dac_windex, dac_rindex;

void vita_dac_out(unsigned short port, unsigned char value)
{
	switch (port) {
	case 0x3c8:
		dac_windex = value * 3;
		break;
	case 0x3c7:
		dac_rindex = value * 3;
		break;
	case 0x3c9:
		dac[dac_windex] = value & 63;
		if (++dac_windex >= 768)
			dac_windex = 0;
		dac_dirty = 1;
		break;
	}
}

unsigned char vita_dac_in(unsigned short port)
{
	static unsigned char retrace;
	unsigned char v;

	switch (port) {
	case 0x3c9:
		v = dac[dac_rindex];
		if (++dac_rindex >= 768)
			dac_rindex = 0;
		return v;
	case 0x3da:
		retrace ^= 8;
		return retrace;
	}
	return 0;
}

// VGA DAC is 6 bits per gun, the P8 palette wants 0xAABBGGRR
static void build_palette(uint *pal)
{
	int i;
	uint r, g, b;

	dac_dirty = 0;
	for (i = 0; i < 256; i++) {
		r = (dac[i*3+0] << 2) | (dac[i*3+0] >> 4);
		g = (dac[i*3+1] << 2) | (dac[i*3+1] >> 4);
		b = (dac[i*3+2] << 2) | (dac[i*3+2] >> 4);
		pal[i] = 0xff000000 | (b << 16) | (g << 8) | r;
	}
}

// only the 8-bit screen goes up, the GPU does the palette lookup and the scaling
static void upload_screen(vita2d_texture *tex, int *w, int *h)
{
	grs_bitmap *bm;
	ubyte *src, *dst;
	int y, stride;

	sceKernelLockLwMutex(&screen_lock, 1, NULL);

	if (!gr_installed || grd_curscreen == NULL) {
		sceKernelUnlockLwMutex(&screen_lock, 1);
		*w = *h = 0;
		return;
	}

	if (dac_dirty)
		build_palette(vita2d_texture_get_palette(tex));

	bm = &grd_curscreen->sc_canvas.cv_bitmap;
	src = show_start;
	dst = vita2d_texture_get_datap(tex);
	stride = vita2d_texture_get_stride(tex);
	for (y = 0; y < bm->bm_h && src + bm->bm_w <= screen_buffer + sizeof(screen_buffer); y++, src += bm->bm_rowsize)
		memcpy(dst + y * stride, src, bm->bm_w);

	*w = bm->bm_w;
	*h = bm->bm_h;

	sceKernelUnlockLwMutex(&screen_lock, 1);
}

static int scanout_main(SceSize args, void *argp)
{
	vita2d_texture *tex;
	SceGxmTextureFilter filter;
	float x, y, sx, sy;
	int w, h;

	vita2d_init();
	vita2d_set_clear_color(RGBA8(0, 0, 0, 255));
	tex = vita2d_create_empty_texture_format(MAX_SCREEN_W, MAX_SCREEN_H, SCE_GXM_TEXTURE_FORMAT_P8_ABGR);

	while (scanout_running) {
		// the GPU has to be done with last frame's texture before it gets overwritten
		vita2d_wait_rendering_done();

		upload_screen(tex, &w, &h);

		filter = vita_smooth ? SCE_GXM_TEXTURE_FILTER_LINEAR : SCE_GXM_TEXTURE_FILTER_POINT;
		vita2d_texture_set_filters(tex, filter, filter);

		if (vita_screen_mode == VITA_SCREEN_STRETCH) {
			sx = (float)DISPLAY_W / w;
			x = 0;
		} else {
			sx = (DISPLAY_H * 4.0f / 3.0f) / w;
			x = (DISPLAY_W - w * sx) / 2;
		}
		sy = (float)DISPLAY_H / h;
		y = 0;

		vita2d_start_drawing();
		vita2d_clear_screen();
		if (w > 0)
			vita2d_draw_texture_part_scale(tex, x, y, 0, 0, w, h, sx, sy);
		vita2d_end_drawing();
		vita2d_common_dialog_update();
		vita2d_swap_buffers();
	}

	vita2d_wait_rendering_done();
	vita2d_free_texture(tex);
	vita2d_fini();

	return sceKernelExitDeleteThread(0);
}

void vita_video_init(void)
{
	if (scanout_thid >= 0)
		return;

	sceKernelCreateLwMutex(&screen_lock, "screen_lock", 0, 0, NULL);

	gr_video_memory = screen_buffer;

	scanout_running = 1;
	scanout_thid = sceKernelCreateThread("scanout", scanout_main, 0x10000100 - 5, 0x10000, 0, SCE_KERNEL_CPU_MASK_USER_1, NULL);
	sceKernelStartThread(scanout_thid, 0, NULL);
}

void vita_video_shutdown(void)
{
	if (scanout_thid < 0)
		return;
	scanout_running = 0;
	sceKernelWaitThreadEnd(scanout_thid, NULL, NULL);
	scanout_thid = -1;
}

// The VGA flip waited for retrace, which is what kept the automap from
// clearing a page while it was still being scanned out. Same deal here.
void vita_show_canvas(grs_canvas *canv)
{
	ubyte *p = canv->cv_bitmap.bm_data;

	if (p >= screen_buffer && p < screen_buffer + sizeof(screen_buffer)) {
		show_start = p;
		sceDisplayWaitVblankStart();
	}
}

void vita_video_lock(void)
{
	sceKernelLockLwMutex(&screen_lock, 1, NULL);
}

void vita_video_unlock(void)
{
	sceKernelUnlockLwMutex(&screen_lock, 1);
}

static int mode_size(int mode, int *w, int *h)
{
	switch (mode) {
	case SM_320x200C:
	case SM_320x200U:	*w = 320; *h = 200; return 0;
	case SM_320x240U:	*w = 320; *h = 240; return 0;
	case SM_320x400U:	*w = 320; *h = 400; return 0;
	case SM_640x400V:	*w = 640; *h = 400; return 0;
	case SM_640x480V:	*w = 640; *h = 480; return 0;
	}
	return 11;
}

int gr_check_mode(int mode)
{
	int w, h;

	return mode_size(mode, &w, &h);
}

int gr_set_mode(int mode)
{
	int w, h, retcode;

	if (mode == SM_ORIGINAL)
		return 0;

	if ((retcode = mode_size(mode, &w, &h)) != 0)
		return retcode;

	vita_video_lock();

	gr_palette_clear();
	memset(screen_buffer, 0, sizeof(screen_buffer));
	show_start = screen_buffer;

	memset(grd_curscreen, 0, sizeof(grs_screen));
	grd_curscreen->sc_mode = mode;
	grd_curscreen->sc_w = w;
	grd_curscreen->sc_h = h;
	grd_curscreen->sc_aspect = fixdiv(grd_curscreen->sc_w*3,grd_curscreen->sc_h*4);
	grd_curscreen->sc_canvas.cv_bitmap.bm_x = 0;
	grd_curscreen->sc_canvas.cv_bitmap.bm_y = 0;
	grd_curscreen->sc_canvas.cv_bitmap.bm_w = w;
	grd_curscreen->sc_canvas.cv_bitmap.bm_h = h;
	grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize = w;
	grd_curscreen->sc_canvas.cv_bitmap.bm_type = BM_LINEAR;
	grd_curscreen->sc_canvas.cv_bitmap.bm_data = screen_buffer;
	gr_set_current_canvas(NULL);

	vita_video_unlock();

	return 0;
}

int gr_init_A0000()
{
	return 0;
}

int gr_save_mode()
{
	return 0;
}

void gr_restore_mode()
{
	gr_palette_fade_out( gr_palette, 32, 0 );
}

int gr_init(int mode)
{
	int retcode;

	if (gr_installed == 1)
		return 1;

	vita_video_init();

	grd_curscreen = (grs_screen *)malloc(sizeof(grs_screen));
	memset(grd_curscreen, 0, sizeof(grs_screen));

	if ((retcode = gr_set_mode(mode)) != 0)
		return retcode;

	grd_curscreen->sc_canvas.cv_color = 0;
	grd_curscreen->sc_canvas.cv_drawmode = 0;
	grd_curscreen->sc_canvas.cv_font = NULL;
	grd_curscreen->sc_canvas.cv_font_fg_color = 0;
	grd_curscreen->sc_canvas.cv_font_bg_color = 0;
	gr_set_current_canvas( &grd_curscreen->sc_canvas );

	gr_installed = 1;
	atexit((void (*)(void))gr_close);

	return 0;
}

int gr_close()
{
	if (gr_installed == 1) {
		gr_installed = 0;
		vita_video_shutdown();
		free(grd_curscreen);
		grd_curscreen = NULL;
	}
	return 0;
}

void gr_sync_display()
{
	sceDisplayWaitVblankStart();
}



int get_selector( void * address, int size, unsigned int * selector )
{
	*selector = 1;
	return 0;
}

int gr_modex_setmode(short mode)
{
	return 0;
}

void gr_modex_setplane(short plane)
{
}

void gr_modex_setstart(short x, short y, int wait_for_retrace)
{
}

void gr_modex_uscanline( short x1, short x2, short y, unsigned char color )
{
}

void gr_modex_line()
{
}

int gr_vesa_setmodea(int mode)
{
	return 11;
}

int gr_vesa_checkmode(int mode)
{
	return 11;
}

void gr_vesa_setstart(short x, short y )
{
}

void gr_vesa_setpage(int page)
{
}

void gr_vesa_incpage()
{
}

void gr_vesa_scanline(short x1, short x2, short y, unsigned char color )
{
}

int gr_vesa_setlogical(int pixels_per_scanline)
{
	return 0;
}

void gr_vesa_bitblt( unsigned char * source_ptr, unsigned int vesa_address, int height, int width )
{
}

void gr_vesa_pixel( unsigned char color, unsigned int offset )
{
}

void gr_vesa_bitmap( grs_bitmap * source, grs_bitmap * dest, int x, int y )
{
}

void gr_vesa_update( grs_bitmap * source1, grs_bitmap * dest, grs_bitmap * source2 )
{
}
