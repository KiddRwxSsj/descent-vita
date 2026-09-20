#include "globvars.h"

extern void scale_bitmap(grs_bitmap *bp, grs_point *vertbuf);

fix blob_vertices[4*2];

static g3s_point rod_points[4];
static g3s_point *rod_point_list[4] = { &rod_points[0], &rod_points[1], &rod_points[2], &rod_points[3] };

// half a pixel in from the edges so the texture mapper never reads past the bitmap
static g3s_uvl uvl_list[4] = {
	{ 0x0200, 0x0200, 0 },
	{ 0xfe00, 0x0200, 0 },
	{ 0xfe00, 0xfe00, 0 },
	{ 0x0200, 0xfe00, 0 },
};

static ubyte calc_rod_corners(g3s_point *bot_point,fix bot_width,g3s_point *top_point,fix top_width)
{
	vms_vector delta_vec, top, rod_norm, temp_vec;
	ubyte codes_and;
	int i;

	vm_vec_sub(&delta_vec, &bot_point->p3_vec, &top_point->p3_vec);

	delta_vec.x = fixdiv(delta_vec.x, Matrix_scale.x);
	delta_vec.y = fixdiv(delta_vec.y, Matrix_scale.y);

	vm_vec_normalize(&delta_vec);
	top = top_point->p3_vec;
	vm_vec_normalize(&top);

	vm_vec_crossprod(&rod_norm, &delta_vec, &top);
	vm_vec_normalize(&rod_norm);

	rod_norm.x = fixmul(rod_norm.x, Matrix_scale.x);
	rod_norm.y = fixmul(rod_norm.y, Matrix_scale.y);

	vm_vec_copy_scale(&temp_vec, &rod_norm, top_width);
	temp_vec.z = 0;
	vm_vec_add(&rod_points[0].p3_vec, &top_point->p3_vec, &temp_vec);
	vm_vec_sub(&rod_points[1].p3_vec, &top_point->p3_vec, &temp_vec);

	vm_vec_copy_scale(&temp_vec, &rod_norm, bot_width);
	temp_vec.z = 0;
	vm_vec_sub(&rod_points[2].p3_vec, &bot_point->p3_vec, &temp_vec);
	vm_vec_add(&rod_points[3].p3_vec, &bot_point->p3_vec, &temp_vec);

	codes_and = 0xff;
	for (i = 0; i < 4; i++)
		codes_and &= g3_code_point(&rod_points[i]);

	if (codes_and)
		return codes_and;

	for (i = 0; i < 4; i++)
		rod_points[i].p3_flags = 0;

	return 0;
}

g3_draw_rod_flat(g3s_point *bot_point,fix bot_width,g3s_point *top_point,fix top_width)
{
	if (calc_rod_corners(bot_point, bot_width, top_point, top_width))
		return 0;
	return g3_draw_poly(4, rod_point_list);
}

g3_draw_rod_tmap(grs_bitmap *bitmap,g3s_point *bot_point,fix bot_width,g3s_point *top_point,fix top_width,fix light)
{
	int i;

	for (i = 0; i < 4; i++)
		uvl_list[i].l = light;

	if (calc_rod_corners(bot_point, bot_width, top_point, top_width))
		return 0;
	return g3_draw_tmap(4, rod_point_list, uvl_list, bitmap);
}

g3_draw_bitmap(vms_vector *pos,fix width,fix height,grs_bitmap *bm)
{
	g3s_point *pnt = &rod_points[0];
	long long n;
	fix w, h;

	if (g3_rotate_point(pnt, pos) & CC_BEHIND)
		return 0;

	g3_project_point(pnt);
	if (pnt->p3_flags & PF_OVERFLOW)
		return 0;

	n = (long long)width * Canv_w2;
	if (g3_div_overflow(n, pnt->z))
		return 0;
	w = fixmul(g3_div(n, pnt->z), Matrix_scale.x);

	n = (long long)height * Canv_h2;
	if (g3_div_overflow(n, pnt->z))
		return 0;
	h = fixmul(g3_div(n, pnt->z), Matrix_scale.y);

	blob_vertices[0] = pnt->p3_sx - w;
	blob_vertices[2] = blob_vertices[4] = pnt->p3_sx + w;
	blob_vertices[1] = blob_vertices[3] = pnt->p3_sy - h;
	blob_vertices[5] = pnt->p3_sy + h;

	scale_bitmap(bm, (grs_point *)blob_vertices);
	return 0;
}
