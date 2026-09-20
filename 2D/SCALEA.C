#include <string.h>

#include "gr.h"

#define MAX_CC_RUNS	1024

char scale_trans_color;
int scale_error_term;
int scale_initial_pixel_count;
int scale_adj_up;
int scale_adj_down;
int scale_final_pixel_count;
int scale_ydelta_minus_1;
int scale_whole_step;
ubyte *scale_source_ptr;
ubyte *scale_dest_ptr;

// The asm patched run lengths straight into an unrolled loop; a table of
// precomputed run lengths does the same job without self-modifying code.
static int cc_runs[MAX_CC_RUNS];
static int cc_count;

static inline void put_run(ubyte **d, int n, ubyte c)
{
	if (c != (ubyte)scale_trans_color)
		memset(*d, c, n);
	*d += n;
}

void rls_do_cc_setup_asm(void)
{
	int i, err = scale_error_term;

	cc_count = scale_ydelta_minus_1;
	if (cc_count > MAX_CC_RUNS)
		cc_count = MAX_CC_RUNS;

	for (i = 0; i < cc_count; i++) {
		err += scale_adj_up;
		if (err > 0) {
			cc_runs[i] = scale_whole_step + 1;
			err -= scale_adj_down;
		} else
			cc_runs[i] = scale_whole_step;
	}
}

void scale_do_cc_scanline(void)
{
	ubyte *s = scale_source_ptr;
	ubyte *d = scale_dest_ptr;
	int i;

	put_run(&d, scale_initial_pixel_count, *s++);
	for (i = 0; i < cc_count; i++)
		put_run(&d, cc_runs[i], *s++);
	put_run(&d, scale_final_pixel_count, *s);
}

void rls_stretch_scanline_asm(void)
{
	ubyte *s = scale_source_ptr;
	ubyte *d = scale_dest_ptr;
	int i, run, err = scale_error_term;

	put_run(&d, scale_initial_pixel_count, *s++);
	for (i = 0; i < scale_ydelta_minus_1; i++) {
		run = scale_whole_step;
		if ((err += scale_adj_up) > 0) {
			run++;
			err -= scale_adj_down;
		}
		put_run(&d, run, *s++);
	}
	put_run(&d, scale_final_pixel_count, *s);
}
