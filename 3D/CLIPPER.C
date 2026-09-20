#include "globvars.h"
#include "error.h"

static g3s_point temp_points[MAX_POINTS_IN_POLY];
static g3s_point *free_points[MAX_POINTS_IN_POLY];
int free_point_num = -1;

g3s_point *get_temp_point(void)
{
	g3s_point *p;
	int i;

	if (free_point_num < 0) {
		for (i = 0; i < MAX_POINTS_IN_POLY; i++)
			free_points[i] = &temp_points[i];
		free_point_num = 0;
	}

	Assert(free_point_num < MAX_POINTS_IN_POLY);

	p = free_points[free_point_num++];
	p->p3_flags = PF_TEMP_POINT;
	return p;
}

void free_temp_point(g3s_point *p)
{
	free_points[--free_point_num] = p;
}

static inline fix clip_value(fix s, fix e, fix num, fix den)
{
	return g3_div((long long)(e - s) * num, den) + s;
}

// on = point on screen, off = point off the given plane.  Returns new point.
static g3s_point *clip_edge(g3s_point *on, g3s_point *off, ubyte plane)
{
	fix a, b, num, den;
	g3s_point *n;

	if (plane & (CC_OFF_TOP|CC_OFF_BOT)) {
		a = on->y;
		b = off->y;
	} else {
		a = on->x;
		b = off->x;
	}
	if (plane & (CC_OFF_LEFT|CC_OFF_BOT)) {
		a = -a;
		b = -b;
	}

	num = a - on->z;
	den = num - b + off->z;

	n = get_temp_point();

	n->x = clip_value(on->x, off->x, num, den);
	n->z = n->x;
	n->y = clip_value(on->y, off->y, num, den);
	if (plane & (CC_OFF_TOP|CC_OFF_BOT))
		n->z = n->y;

	if (on->p3_flags & PF_UVS) {
		n->p3_u = clip_value(on->p3_u, off->p3_u, num, den);
		n->p3_v = clip_value(on->p3_v, off->p3_v, num, den);
		n->p3_flags |= PF_UVS;
	}
	if (on->p3_flags & PF_LVS) {
		n->p3_l = clip_value(on->p3_l, off->p3_l, num, den);
		n->p3_flags |= PF_LVS;
	}

	if (plane & (CC_OFF_LEFT|CC_OFF_BOT))
		n->z = -n->z;

	g3_code_point(n);
	return n;
}

void clip_line(g3s_point **p0, g3s_point **p1, ubyte codes_or)
{
	ubyte plane;
	g3s_point *on, *off, *old;

	for (plane = 1; plane < 16; plane <<= 1) {
		if (!(codes_or & plane))
			continue;

		if ((*p0)->p3_codes & plane) {
			old = *p0; *p0 = *p1; *p1 = old;
		}
		on = *p0;
		off = *p1;

		*p1 = clip_edge(on, off, plane);

		if (off->p3_flags & PF_TEMP_POINT)
			free_temp_point(off);

		if (on->p3_codes & (*p1)->p3_codes)
			return;
		codes_or = on->p3_codes | (*p1)->p3_codes;
	}
}

static int clip_plane(g3s_point **src, g3s_point **dest, int nv, ubyte plane, g3s_codes *cc)
{
	g3s_point **d = dest;
	g3s_point *cur, *prev, *next, *n;
	int i;

	src[nv] = src[0];
	src[nv+1] = src[1];

	cc->and = 0xff;
	cc->or = 0;

	for (i = 1; i <= nv; i++) {
		cur = src[i];

		if (cur->p3_codes & plane) {
			prev = src[i-1];
			if (!(prev->p3_codes & plane)) {
				n = clip_edge(prev, cur, plane);
				*d++ = n;
				cc->or |= n->p3_codes;
				cc->and &= n->p3_codes;
			}

			next = src[i+1];
			if (!(next->p3_codes & plane)) {
				n = clip_edge(next, cur, plane);
				*d++ = n;
				cc->or |= n->p3_codes;
				cc->and &= n->p3_codes;
			}

			if (cur->p3_flags & PF_TEMP_POINT)
				free_temp_point(cur);
		} else {
			*d++ = cur;
			cc->or |= cur->p3_codes;
			cc->and &= cur->p3_codes;
		}
	}

	return d - dest;
}

g3s_point **clip_polygon(g3s_point **src, g3s_point **dest, int *nv, g3s_codes *cc)
{
	g3s_point **t;
	ubyte plane;

	for (plane = 1; plane < 16; plane <<= 1) {
		if (!(cc->or & plane))
			continue;

		*nv = clip_plane(src, dest, *nv, plane, cc);

		t = src; src = dest; dest = t;

		if (cc->and)
			break;
	}

	return src;
}
