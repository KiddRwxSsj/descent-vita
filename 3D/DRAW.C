#include "globvars.h"
#include "texmap.h"

extern void gr_upoly_tmap(int nverts, int *vert);

typedef void (*tmap_drawer_t)(grs_bitmap *bm, int nv, g3s_point **vertlist);
typedef void (*flat_drawer_t)(int nv, int *vertlist);
typedef int (*line_drawer_t)(fix x0, fix y0, fix x1, fix y1);

static tmap_drawer_t tmap_drawer_ptr = (tmap_drawer_t)draw_tmap;
static flat_drawer_t flat_drawer_ptr = gr_upoly_tmap;
static line_drawer_t line_drawer_ptr = gr_line;

g3_set_special_render(void *tmap_drawer(),void *flat_drawer(),void *line_drawer())
{
	tmap_drawer_ptr = tmap_drawer ? (tmap_drawer_t)tmap_drawer : (tmap_drawer_t)draw_tmap;
	flat_drawer_ptr = flat_drawer ? (flat_drawer_t)flat_drawer : gr_upoly_tmap;
	line_drawer_ptr = line_drawer ? (line_drawer_t)line_drawer : gr_line;
	return 0;
}

bool g3_draw_line(g3s_point *p0,g3s_point *p1)
{
	ubyte codes_or;
	bool ret;

	if (p0->p3_codes & p1->p3_codes)
		return 0;

	codes_or = p0->p3_codes | p1->p3_codes;

	if (!(codes_or & CC_BEHIND)) {
		if (!(p0->p3_flags & PF_PROJECTED))
			g3_project_point(p0);
		if (!(p0->p3_flags & PF_OVERFLOW)) {
			if (!(p1->p3_flags & PF_PROJECTED))
				g3_project_point(p1);
			if (!(p1->p3_flags & PF_OVERFLOW)) {
				ret = (*line_drawer_ptr)(p0->p3_sx, p0->p3_sy, p1->p3_sx, p1->p3_sy);
				return codes_or ? ret : 1;
			}
		}
	}

	clip_line(&p0, &p1, codes_or);
	ret = g3_draw_line(p0, p1);

	if (p0->p3_flags & PF_TEMP_POINT)
		free_temp_point(p0);
	if (p1->p3_flags & PF_TEMP_POINT)
		free_temp_point(p1);

	return ret;
}

bool g3_check_normal_facing(vms_vector *v,vms_vector *norm)
{
	vms_vector t;

	vm_vec_sub(&t, &View_position, v);
	return vm_vec_dotprod(&t, norm) > 0;
}

static bool do_facing_check(vms_vector *norm, g3s_point **pointlist, vms_vector *pnt)
{
	vms_vector t;

	if (norm)
		return g3_check_normal_facing(pnt, norm);

	vm_vec_perp(&t, &pointlist[0]->p3_vec, &pointlist[1]->p3_vec, &pointlist[2]->p3_vec);
	return vm_vec_dotprod(&t, &pointlist[1]->p3_vec) < 0;
}

static void free_temps(g3s_point **list, int nv)
{
	int i;

	for (i = nv-1; i >= 0; i--)
		if (list[i]->p3_flags & PF_TEMP_POINT)
			free_temp_point(list[i]);
}

bool g3_draw_poly(int nv,g3s_point **pointlist)
{
	g3s_point **list = Vbuf0;
	g3s_codes cc;
	int i, clipped = 0;
	bool ret = 1;

	cc.and = 0xff;
	cc.or = 0;
	for (i = 0; i < nv; i++) {
		Vbuf0[i] = pointlist[i];
		cc.and &= pointlist[i]->p3_codes;
		cc.or |= pointlist[i]->p3_codes;
	}

	if (cc.and)
		return 1;

	if (cc.or) {
		list = clip_polygon(Vbuf0, Vbuf1, &nv, &cc);
		clipped = 1;
		if (nv == 0 || cc.and || (cc.or & CC_BEHIND))
			goto free_points;
	}

	for (i = 0; i < nv; i++) {
		g3s_point *p = list[i];

		if (!(p->p3_flags & PF_PROJECTED))
			g3_project_point(p);
		if (p->p3_flags & PF_OVERFLOW)
			goto free_points;

		Vertex_list[i*2] = p->p3_sx;
		Vertex_list[i*2+1] = p->p3_sy;
	}

	(*flat_drawer_ptr)(nv, (int *)Vertex_list);
	ret = 0;

free_points:
	if (clipped)
		free_temps(list, nv);
	return ret;
}

bool g3_check_and_draw_poly(int nv,g3s_point **pointlist,vms_vector *norm,vms_vector *pnt)
{
	if (do_facing_check(norm, pointlist, pnt))
		return g3_draw_poly(nv, pointlist);
	return 255;
}

bool g3_draw_tmap(int nv,g3s_point **pointlist,g3s_uvl *uvl_list,grs_bitmap *bm)
{
	g3s_point **list = Vbuf0;
	g3s_codes cc;
	int i, clipped = 0;
	bool ret = 1;

	cc.and = 0xff;
	cc.or = 0;
	for (i = 0; i < nv; i++) {
		g3s_point *p = pointlist[i];

		Vbuf0[i] = p;
		cc.and &= p->p3_codes;
		cc.or |= p->p3_codes;
		p->p3_u = uvl_list[i].u;
		p->p3_v = uvl_list[i].v;
		p->p3_l = uvl_list[i].l;
		p->p3_flags |= PF_UVS | PF_LVS;
	}

	if (cc.and)
		return 1;

	if (cc.or) {
		list = clip_polygon(Vbuf0, Vbuf1, &nv, &cc);
		clipped = 1;
		if (nv == 0 || cc.and || (cc.or & CC_BEHIND))
			goto free_points;
	}

	for (i = 0; i < nv; i++) {
		if (!(list[i]->p3_flags & PF_PROJECTED))
			g3_project_point(list[i]);
		if (list[i]->p3_flags & PF_OVERFLOW)
			goto free_points;
	}

	(*tmap_drawer_ptr)(bm, nv, list);
	ret = 0;

free_points:
	if (clipped)
		free_temps(list, nv);
	return ret;
}

bool g3_check_and_draw_tmap(int nv,g3s_point **pointlist,g3s_uvl *uvl_list,grs_bitmap *bm,vms_vector *norm,vms_vector *pnt)
{
	if (do_facing_check(norm, pointlist, pnt))
		return g3_draw_tmap(nv, pointlist, uvl_list, bm);
	return 255;
}

g3_draw_sphere(g3s_point *pnt,fix rad)
{
	long long n;

	if (pnt->p3_codes & CC_BEHIND)
		return 0;

	if (!(pnt->p3_flags & PF_PROJECTED))
		g3_project_point(pnt);
	if (pnt->p3_flags & PF_OVERFLOW)
		return 0;

	n = (long long)fixmul(rad, Matrix_scale.x) * Canv_w2;
	if (g3_div_overflow(n, pnt->z))
		return 0;

	gr_disk(pnt->p3_sx, pnt->p3_sy, g3_div(n, pnt->z));
	return 0;
}
