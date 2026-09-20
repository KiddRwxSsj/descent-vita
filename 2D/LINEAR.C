#include <string.h>

#include "gr.h"
#include "grdef.h"

unsigned int gr_var_color;
unsigned char *gr_var_bitmap;
unsigned int gr_var_bwidth;

static inline void draw_run(ubyte **p, int n, int step, ubyte c)
{
	ubyte *d = *p;

	while (n-- > 0) {
		*d = c;
		d += step;
	}
	*p = d;
}

// Abrash run-length slice line, same pixel choices as the asm version
void gr_linear_line( int x0, int y0, int x1, int y1 )
{
	int bw = gr_var_bwidth;
	ubyte c = gr_var_color;
	ubyte *p;
	int xdelta, ydelta, xadv, t;
	int whole, adj_up, adj_down, err, initial, final, run, i;

	if (y0 > y1) {
		t = y0; y0 = y1; y1 = t;
		t = x0; x0 = x1; x1 = t;
	}

	p = gr_var_bitmap + y0 * bw + x0;
	ydelta = y1 - y0;
	xdelta = x1 - x0;

	if (xdelta == 0) {
		draw_run(&p, ydelta + 1, bw, c);
		return;
	}

	xadv = 1;
	if (xdelta < 0) {
		xadv = -1;
		xdelta = -xdelta;
	}

	if (ydelta == 0) {
		if (xadv < 0)
			p -= xdelta;
		memset(p, c, xdelta + 1);
		return;
	}

	if (ydelta == xdelta) {
		draw_run(&p, ydelta + 1, bw + xadv, c);
		return;
	}

	if (xdelta >= ydelta) {
		whole = xdelta / ydelta;
		adj_up = (xdelta % ydelta) * 2;
		adj_down = ydelta * 2;
		err = (xdelta % ydelta) - adj_down;
		initial = (whole / 2) + 1;
		final = initial;
		if ((adj_up == 0) && ((whole & 1) == 0))
			initial--;
		if (whole & 1)
			err += ydelta;

		draw_run(&p, initial, xadv, c);
		p += bw;

		for (i = 0; i < ydelta - 1; i++) {
			run = whole;
			if ((err += adj_up) > 0) {
				run++;
				err -= adj_down;
			}
			draw_run(&p, run, xadv, c);
			p += bw;
		}

		draw_run(&p, final, xadv, c);
	} else {
		whole = ydelta / xdelta;
		adj_up = (ydelta % xdelta) * 2;
		adj_down = xdelta * 2;
		err = (ydelta % xdelta) - adj_down;
		initial = (whole / 2) + 1;
		final = initial;
		if ((adj_up == 0) && ((whole & 1) == 0))
			initial--;
		if (whole & 1)
			err += xdelta;

		draw_run(&p, initial, bw, c);
		p += xadv;

		for (i = 0; i < xdelta - 1; i++) {
			run = whole;
			if ((err += adj_up) > 0) {
				run++;
				err -= adj_down;
			}
			draw_run(&p, run, bw, c);
			p += xadv;
		}

		draw_run(&p, final, bw, c);
	}
}

void gr_linear_stosd( void * dest, unsigned char color, int nbytes)
{
	memset(dest, color, nbytes);
}

void gr_update_buffer( void * sbuf1, void * sbuf2, void * dbuf, int size )
{
	uint *s1 = sbuf1, *s2 = sbuf2, *d = dbuf;
	int i, n = size >> 2;

	for (i = 0; i < n; i++)
		if (s1[i] != s2[i])
			d[i] = s1[i];
	if (n > 0)
		d[n-1] = s1[n-1];
}
