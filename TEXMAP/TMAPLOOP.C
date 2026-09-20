#include "fix.h"
#include "gr.h"
#include "grdef.h"
#include "texmap.h"
#include "texmapl.h"

extern int Lighting_on;
extern int Transparency_on;
extern int y_pointers[];

#define PER_SPAN_BITS	4
#define PER_SPAN		(1 << PER_SPAN_BITS)

#define TEXEL(u,v)	((((v) >> 10) & (63 << 6)) | (((u) >> 16) & 63))

static inline ubyte *scan_dest(int x)
{
	return (ubyte *)write_buffer + y_pointers[fx_y] + x;
}

// lighting comes in as 16.16, the inner loops want 8.8 so the fade level
// is simply the high byte
static inline void light_8_8(fix *l, fix *dl)
{
	*l = fx_l >> 8;
	*dl = fx_dl_dx >> 8;
	if (*dl < 0)
		(*dl)++;
}

static inline fix per_div(fix n, fix z)
{
	double q = ((double)n * 65536.0) / (double)z;

	if (q > 2147483647.0) return 0x7fffffff;
	if (q < -2147483647.0) return -0x7fffffff;
	return (fix)q;
}

void asm_tmap_scanline_per(void)
{
	ubyte *dest, *src = pixptr;
	fix u, v, z, l, dl, du_span, dv_span, dz_span;
	fix U0, V0, U1, V1, dU, dV;
	int x, n, len, i;
	ubyte c;

	x = (fx_xleft < 0) ? 0 : fx_xleft;
	n = fx_xright - x + 1;
	if (n <= 0)
		return;

	dest = scan_dest(x);
	light_8_8(&l, &dl);

	u = fx_u;
	v = fx_v;
	z = fx_z;
	if (z == 0)
		return;

	du_span = fx_du_dx << PER_SPAN_BITS;
	dv_span = fx_dv_dx << PER_SPAN_BITS;
	dz_span = fx_dz_dx << PER_SPAN_BITS;

	U0 = per_div(u, z);
	V0 = per_div(v, z);

	while (n > 0) {
		if (n >= PER_SPAN) {
			len = PER_SPAN;
			u += du_span;
			v += dv_span;
			z += dz_span;
		} else {
			len = n;
			u += fx_du_dx * len;
			v += fx_dv_dx * len;
			z += fx_dz_dx * len;
		}
		n -= len;

		if (z == 0)
			return;

		U1 = per_div(u, z);
		V1 = per_div(v, z);
		if (len == PER_SPAN) {
			dU = (U1 - U0) >> PER_SPAN_BITS;
			dV = (V1 - V0) >> PER_SPAN_BITS;
		} else {
			dU = (U1 - U0) / len;
			dV = (V1 - V0) / len;
		}

		if (!Lighting_on) {
			if (!Transparency_on) {
				for (i = len; i > 0; i--) {
					*dest++ = src[TEXEL(U0, V0)];
					U0 += dU; V0 += dV;
				}
			} else {
				for (i = len; i > 0; i--) {
					c = src[TEXEL(U0, V0)];
					if (c != 255)
						*dest = c;
					dest++;
					U0 += dU; V0 += dV;
				}
			}
		} else {
			if (!Transparency_on) {
				for (i = len; i > 0; i--) {
					*dest++ = gr_fade_table[(l & 0xff00) | src[TEXEL(U0, V0)]];
					l += dl;
					U0 += dU; V0 += dV;
				}
			} else {
				// the original only stepped the light on pixels it actually drew
				for (i = len; i > 0; i--) {
					c = src[TEXEL(U0, V0)];
					if (c != 255) {
						*dest = gr_fade_table[(l & 0xff00) | c];
						l += dl;
					}
					dest++;
					U0 += dU; V0 += dV;
				}
			}
		}

		U0 = U1;
		V0 = V1;
	}
}

static int lin_span(int *xstart)
{
	int x0, x1, n;

	if ((uint)fx_y > (uint)window_bottom)
		return -1;

	x0 = (fx_xleft < 0) ? 0 : fx_xleft;
	x1 = fx_xright;
	if (x1 > window_right) x1 = window_right;
	if (x1 < window_left) x1 = window_left;

	n = x1 - x0;
	if (n > window_width)
		n = window_width;

	*xstart = x0;
	return n;
}

void asm_tmap_scanline_lin(void)
{
	ubyte *dest, *src = pixptr;
	fix u, v, du, dv;
	int x, n;
	ubyte c;

	n = lin_span(&x);
	if (n < 0)
		return;

	dest = scan_dest(x);
	u = fx_u;
	v = fx_v;
	du = fx_du_dx;
	dv = fx_dv_dx;

	if (!Transparency_on) {
		for (n++; n > 0; n--) {
			*dest++ = src[TEXEL(u, v)];
			u += du; v += dv;
		}
	} else {
		// transparent spans were always one pixel short in the asm, kept for parity
		for (; n > 0; n--) {
			c = src[TEXEL(u, v)];
			if (c != 255)
				*dest = c;
			dest++;
			u += du; v += dv;
		}
	}
}

void asm_tmap_scanline_lin_lighted(void)
{
	ubyte *dest, *src = pixptr;
	fix u, v, du, dv, l, dl;
	int x, n;
	ubyte c;

	n = lin_span(&x);
	if (n < 0)
		return;

	dest = scan_dest(x);
	u = fx_u;
	v = fx_v;
	du = fx_du_dx;
	dv = fx_dv_dx;
	light_8_8(&l, &dl);

	if (!Transparency_on) {
		for (n++; n > 0; n--) {
			*dest++ = gr_fade_table[(l & 0xff00) | src[TEXEL(u, v)]];
			l += dl;
			u += du; v += dv;
		}
	} else {
		for (n++; n > 0; n--) {
			c = src[TEXEL(u, v)];
			if (c != 255) {
				*dest = gr_fade_table[(l & 0xff00) | c];
				l += dl;
			}
			dest++;
			u += du; v += dv;
		}
	}
}

void asm_tmap_scanline_flat(void)
{
	ubyte *dest, *t;
	int x, n;

	n = lin_span(&x);
	if (n < 0)
		return;
	if (n == window_width)
		n--;

	dest = scan_dest(x);

	if (tmap_flat_cthru_table == NULL) {
		memset(dest, tmap_flat_color, n + 1);
		return;
	}

	t = tmap_flat_cthru_table;
	for (; n > 0; n--) {
		*dest = t[*dest];
		dest++;
	}
}

void asm_tmap_scanline_shaded(void)
{
	ubyte *dest, *fade;
	int x, n;

	if ((uint)fx_y >= (uint)window_bottom)
		return;

	n = lin_span(&x);
	if (n < 0)
		return;

	dest = scan_dest(x);
	fade = &gr_fade_table[(tmap_flat_shade_value & 31) << 8];

	for (n++; n > 0; n--) {
		*dest = fade[*dest];
		dest++;
	}
}

// Matt's editor-only mapper; the game never sets Lighting_enabled to 2
void asm_tmap_scanline_matt(void)
{
}
