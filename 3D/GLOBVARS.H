#ifndef _GLOBVARS_H
#define _GLOBVARS_H

#include "3d.h"

#define MAX_POINTS_IN_POLY	100

#define PF_LVS	16

extern vms_vector View_position;
extern fix View_zoom;
extern vms_matrix Unscaled_matrix;
extern vms_matrix View_matrix;
extern vms_vector Window_scale;
extern vms_vector Matrix_scale;

extern int Canvas_width, Canvas_height;
extern fix Canv_w2, Canv_h2;

extern g3s_point *Vbuf0[MAX_POINTS_IN_POLY];
extern g3s_point *Vbuf1[MAX_POINTS_IN_POLY];
extern fix Vertex_list[MAX_POINTS_IN_POLY*2];

extern int free_point_num;

// 64/32 divide without the libgcc call. Below 2^53 the double quotient
// truncates to exactly the integer idiv result.
static inline fix g3_div(long long n, fix d)
{
	if (n < (1LL << 53) && n > -(1LL << 53))
		return (fix)((double)n / (double)d);
	return (fix)(n / d);
}

ubyte g3_code_point(g3s_point *p);

int g3_div_overflow(long long num, fix den);

g3s_point *get_temp_point(void);
void free_temp_point(g3s_point *p);
g3s_point **clip_polygon(g3s_point **src, g3s_point **dest, int *nv, g3s_codes *cc);
void clip_line(g3s_point **p0, g3s_point **p1, ubyte codes_or);

#endif
