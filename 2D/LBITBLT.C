#include <string.h>

#include "gr.h"

void gr_lbitblt( grs_bitmap * source, grs_bitmap * dest, int height, int width )
{
	ubyte *s = source->bm_data;
	ubyte *d = dest->bm_data;

	while (height-- > 0) {
		memcpy(d, s, width);
		s += source->bm_rowsize;
		d += dest->bm_rowsize;
	}
}

void gr_merge_textures( ubyte * lower, ubyte * upper, ubyte * dest )
{
	int i;
	ubyte c;

	for (i = 0; i < 64*64; i++) {
		c = upper[i];
		dest[i] = (c == 255) ? lower[i] : c;
	}
}

void gr_merge_textures_1( ubyte * lower, ubyte * upper, ubyte * dest )
{
	int x, y;
	ubyte c;

	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++) {
			c = upper[64*x + (63-y)];
			if (c == 255)
				c = lower[64*y + x];
			*dest++ = c;
		}
}

void gr_merge_textures_2( ubyte * lower, ubyte * upper, ubyte * dest )
{
	int x, y;
	ubyte c;

	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++) {
			c = upper[64*(63-y) + (63-x)];
			if (c == 255)
				c = lower[64*y + x];
			*dest++ = c;
		}
}

void gr_merge_textures_3( ubyte * lower, ubyte * upper, ubyte * dest )
{
	int x, y;
	ubyte c;

	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++) {
			c = upper[64*(63-x) + y];
			if (c == 255)
				c = lower[64*y + x];
			*dest++ = c;
		}
}
