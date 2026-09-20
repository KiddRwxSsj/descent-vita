#include <stdlib.h>

#include "globvars.h"

extern void gr_upoly_tmap(int nverts, int *vert);

typedef struct horz_end {
	fix x, y;
	int edge;
} horz_end;

static int color_swap;
static int sky_ground_flag;

static horz_end left_point, right_point;

static fix corners[4][2];

static fix horizon_poly[8*2];

static int add_corners(fix *dest, int start_edge, int end_edge)
{
	int n, i, c;

	n = end_edge - start_edge;
	if (n < 0)
		n += 4;

	c = start_edge;
	for (i = 0; i < n; i++) {
		dest[i*2] = corners[c][0];
		dest[i*2+1] = corners[c][1];
		if (++c == 4)
			c = 0;
	}
	return n;
}

static void draw_horz_poly(horz_end *start, horz_end *end)
{
	int n;

	horizon_poly[0] = end->x;
	horizon_poly[1] = end->y;
	horizon_poly[2] = start->x;
	horizon_poly[3] = start->y;

	n = add_corners(&horizon_poly[4], start->edge, end->edge);
	gr_upoly_tmap(n + 2, (int *)horizon_poly);
}

void g3_draw_horizon(int sky_color,int ground_color)
{
	int top_color, bot_color, t;
	fix hx, hy, hz;
	fix up_right, down_right, down_left, up_left;
	fix x2, y2;
	ubyte flags_and, flags_or;
	horz_end *e, tmp;

	color_swap = 0;
	sky_ground_flag = 0;

	if (View_matrix.uvec.y > 0 || (View_matrix.uvec.y == 0 && View_matrix.uvec.x < 0)) {
		top_color = sky_color;
		bot_color = ground_color;
	} else {
		top_color = ground_color;
		bot_color = sky_color;
		color_swap = -1;
	}

	hx = fixmul(fixmul(Unscaled_matrix.rvec.y, Matrix_scale.y), Matrix_scale.z);
	hy = fixmul(fixmul(Unscaled_matrix.uvec.y, Matrix_scale.x), Matrix_scale.z);
	hz = fixmul(fixmul(Unscaled_matrix.fvec.y, Matrix_scale.x), Matrix_scale.y);

	x2 = hx << 1;
	y2 = hy << 1;

	up_right = hz + hx + hy;
	down_right = up_right - y2;
	down_left = down_right - x2;
	up_left = down_left + y2;

	flags_and = 0xff;
	flags_or = 0;
	t = (up_right < 0) ? 0xff : 0;		flags_and &= t; flags_or |= t;
	t = (down_right < 0) ? 0xff : 0;	flags_and &= t; flags_or |= t;
	t = (down_left < 0) ? 0xff : 0;		flags_and &= t; flags_or |= t;
	t = (up_left < 0) ? 0xff : 0;		flags_and &= t; flags_or |= t;

	corners[0][0] = 0;				corners[0][1] = 0;
	corners[1][0] = Canvas_width << 16;	corners[1][1] = 0;
	corners[2][0] = Canvas_width << 16;	corners[2][1] = Canvas_height << 16;
	corners[3][0] = 0;				corners[3][1] = Canvas_height << 16;

	if (flags_and) {
		sky_ground_flag = -1;
		gr_clear_canvas(ground_color);
		return;
	}
	if (!flags_or) {
		sky_ground_flag = 1;
		gr_clear_canvas(sky_color);
		return;
	}


	e = &left_point;

	if ((up_left ^ down_left) < 0 && e != NULL) {
		e->y = labs(fixmuldiv(corners[2][1], up_left, y2));
		e->x = 0;
		e->edge = 0;
		e = (e == &left_point) ? &right_point : NULL;
	}
	if ((up_left ^ up_right) < 0 && e != NULL) {
		e->x = labs(fixmuldiv(corners[2][0], up_left, x2));
		e->y = 0;
		e->edge = 1;
		e = (e == &left_point) ? &right_point : NULL;
	}
	if ((up_right ^ down_right) < 0 && e != NULL) {
		e->y = labs(fixmuldiv(corners[2][1], up_right, y2));
		e->x = corners[2][0];
		e->edge = 2;
		e = (e == &left_point) ? &right_point : NULL;
	}
	if ((down_right ^ down_left) < 0 && e != NULL) {
		e->x = labs(fixmuldiv(corners[2][0], down_left, x2));
		e->y = corners[2][1];
		e->edge = 3;
		e = (e == &left_point) ? &right_point : NULL;
	}

	if (left_point.x > right_point.x) {
		tmp = left_point;
		left_point = right_point;
		right_point = tmp;
	}

	if (top_color != -1) {
		gr_setcolor(top_color);
		draw_horz_poly(&left_point, &right_point);
	}
	if (bot_color != -1) {
		gr_setcolor(bot_color);
		draw_horz_poly(&right_point, &left_point);
	}
}

static void compute_horz_end_vec(horz_end *p, vms_vector *v)
{
	fix xfrac, yfrac, s, num, den, r, mag;
	long long q;

	xfrac = fixdiv(p->x - Canv_w2, Canv_w2);
	yfrac = -fixdiv(p->y - Canv_h2, Canv_h2);

	s = xfrac + yfrac;
	num = fixmul(s, View_matrix.fvec.z) - View_matrix.rvec.z - View_matrix.uvec.z;
	den = View_matrix.rvec.x + View_matrix.uvec.x - fixmul(s, View_matrix.fvec.x);

	if (!(labs(num) < labs(den))) {
		r = fixdiv(den, num);
		q = (long long)r * r + (1LL << 32);
		mag = quad_sqrt((long)(uint)q, (long)(q >> 32));
		v->z = fixdiv(r, mag);
		v->x = fixdiv(f1_0, mag);
	} else {
		r = fixdiv(num, den);
		q = (long long)r * r + (1LL << 32);
		mag = quad_sqrt((long)(uint)q, (long)(q >> 32));
		v->x = fixdiv(r, mag);
		v->z = fixdiv(f1_0, mag);
	}
	v->y = 0;

	q = (long long)v->x * View_matrix.fvec.x + (long long)v->z * View_matrix.fvec.z;
	if (q < 0) {
		v->x = -v->x;
		v->z = -v->z;
	}
}

static void compute_corner_vec(vms_vector *v, int corner)
{
	fix m13, m46, m79, m56, m23, m89;
	fix a, b, c, den;

	if (corner >= 4)
		corner -= 4;

	a = View_matrix.rvec.x;
	b = View_matrix.rvec.y;
	c = View_matrix.rvec.z;
	if (corner == 0 || corner == 3) {
		a = -a; b = -b; c = -c;
	}
	m13 = a - View_matrix.fvec.x;
	m46 = b - View_matrix.fvec.y;
	m79 = c - View_matrix.fvec.z;

	a = View_matrix.uvec.y;
	b = View_matrix.uvec.x;
	c = View_matrix.uvec.z;
	if (corner >= 2) {
		a = -a; b = -b; c = -c;
	}
	m56 = a - View_matrix.fvec.y;
	m23 = b - View_matrix.fvec.x;
	m89 = c - View_matrix.fvec.z;

	den = fixmul(m56, m13) - fixmul(m46, m23);

	v->x = fixdiv(fixmul(m89, m46) - fixmul(m79, m56), den);
	v->y = fixdiv(fixmul(m79, m23) - fixmul(m89, m13), den);
	v->z = f1_0;

	vm_vec_normalize(v);

	if (vm_vec_dotprod(v, &View_matrix.fvec) <= 0) {
		v->x = -v->x;
		v->y = -v->y;
		v->z = -v->z;
	}
}

int g3_compute_sky_polygon(fix *points_2d,vms_vector *vecs)
{
	horz_end *start, *end;
	int n, i;

	if (sky_ground_flag < 0)
		return 0;

	if (sky_ground_flag > 0) {
		for (i = 0; i < 4; i++) {
			points_2d[i*2] = corners[i][0];
			points_2d[i*2+1] = corners[i][1];
			compute_corner_vec(&vecs[i], i);
		}
		return 4;
	}

	start = &left_point;
	end = &right_point;
	if (color_swap) {
		start = &right_point;
		end = &left_point;
	}

	points_2d[0] = end->x;
	points_2d[1] = end->y;
	points_2d[2] = start->x;
	points_2d[3] = start->y;

	compute_horz_end_vec(end, &vecs[0]);
	compute_horz_end_vec(start, &vecs[1]);

	n = add_corners(&points_2d[4], start->edge, end->edge);
	for (i = 0; i < n; i++)
		compute_corner_vec(&vecs[2+i], start->edge + i);

	return n + 2;
}
