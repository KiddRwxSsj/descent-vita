#include "globvars.h"
#include "error.h"

extern int gr_find_closest_color_15bpp(int rgb);

#define OP_EOF			0
#define OP_DEFPOINTS	1
#define OP_FLATPOLY		2
#define OP_TMAPPOLY		3
#define OP_SORTNORM		4
#define OP_RODBM		5
#define OP_SUBCALL		6
#define OP_DEFP_START	7
#define OP_GLOW			8

#define MAX_POINTS_PER_POLY	25

#define W(p)	(*(ushort *)(p))
#define SW(p)	(*(short *)(p))
#define FX(p)	(*(fix *)(p))
#define VP(p)	((vms_vector *)(p))
#define UVL(p)	((g3s_uvl *)(p))

int g3d_interp_outline = 0;
short highest_texture_num;

static grs_bitmap **model_bitmaps;
static vms_angvec *anim_angles;
static vms_vector *morph_points;
static fix model_light;
static fix *glow_values;
static int glow_num = -1;

static vms_angvec zero_angles = {0,0,0};

static g3s_point *Interp_point_list;
static g3s_point *point_list[MAX_POINTS_PER_POLY];

// polygon records pad the vertex list to an odd count of shorts so whatever
// follows stays longword aligned
static inline int vert_list_size(int nv)
{
	return ((nv & ~1) + 1) * 2;
}

void g3_set_interp_points(g3s_point *pointlist)
{
	Interp_point_list = pointlist;
}

static void rotate_point_list(g3s_point *dest, vms_vector *src, int n)
{
	while (n--)
		g3_rotate_point(dest++, src++);
}

static void draw_outline(int nv, ubyte *verts)
{
	int i;

	gr_setcolor(255);
	for (i = 0; i < nv-1; i++)
		g3_draw_line(&Interp_point_list[W(verts+i*2)], &Interp_point_list[W(verts+(i+1)*2)]);
	g3_draw_line(&Interp_point_list[W(verts+i*2)], &Interp_point_list[W(verts)]);
}

static fix surface_light(ubyte *norm)
{
	fix l;

	l = -vm_vec_dotprod(&View_matrix.fvec, VP(norm));
	l = ((l + l + l) >> 2) + f1_0/4;
	return fixmul(l, model_light);
}

static void interp(ubyte *p)
{
	int nv, i;
	fix light;

	for (;;) {
		switch (W(p)) {

		case OP_EOF:
			return;

		case OP_DEFPOINTS:
			nv = W(p+2);
			rotate_point_list(Interp_point_list, VP(p+4), nv);
			p += 4 + nv * sizeof(vms_vector);
			break;

		case OP_DEFP_START:
			nv = W(p+2);
			rotate_point_list(&Interp_point_list[W(p+4)], VP(p+8), nv);
			p += 8 + nv * sizeof(vms_vector);
			break;

		case OP_FLATPOLY:
			nv = W(p+2);
			Assert(nv < MAX_POINTS_PER_POLY);
			if (g3_check_normal_facing(VP(p+4), VP(p+16))) {
				gr_setcolor(W(p+28));
				for (i = 0; i < nv; i++)
					point_list[i] = &Interp_point_list[W(p+30+i*2)];
				g3_draw_poly(nv, point_list);
				if (g3d_interp_outline)
					draw_outline(nv, p+30);
			}
			p += 30 + vert_list_size(nv);
			break;

		case OP_TMAPPOLY:
			nv = W(p+2);
			Assert(nv < MAX_POINTS_PER_POLY);
			if (g3_check_normal_facing(VP(p+4), VP(p+16))) {
				g3s_uvl *uvl_list = UVL(p + 30 + vert_list_size(nv));

				if (glow_num >= 0) {
					light = glow_values[glow_num];
					glow_num = -1;
				} else
					light = surface_light(p+16);

				for (i = 0; i < nv; i++)
					uvl_list[i].l = light;

				for (i = 0; i < nv; i++)
					point_list[i] = &Interp_point_list[W(p+30+i*2)];

				g3_draw_tmap(nv, point_list, uvl_list, model_bitmaps[W(p+28)]);
				if (g3d_interp_outline)
					draw_outline(nv, p+30);
			}
			p += 30 + vert_list_size(nv) + nv * sizeof(g3s_uvl);
			break;

		case OP_SORTNORM:
			if (g3_check_normal_facing(VP(p+16), VP(p+4))) {
				interp(p + W(p+30));
				interp(p + W(p+28));
			} else {
				interp(p + W(p+28));
				interp(p + W(p+30));
			}
			p += 32;
			break;

		case OP_RODBM: {
			g3s_point rod_bot_p, rod_top_p;

			g3_rotate_point(&rod_bot_p, VP(p+20));
			g3_rotate_point(&rod_top_p, VP(p+4));
			g3_draw_rod_tmap(model_bitmaps[W(p+2)], &rod_bot_p, FX(p+16), &rod_top_p, FX(p+32), f1_0);
			p += 36;
			break;
		}

		case OP_SUBCALL: {
			vms_angvec *a = anim_angles ? &anim_angles[W(p+2)] : &zero_angles;

			g3_start_instance_angles(VP(p+4), a);
			interp(p + W(p+16));
			g3_done_instance();
			p += 20;
			break;
		}

		case OP_GLOW:
			if (glow_values)
				glow_num = W(p+2);
			p += 4;
			break;

		default:
			Int3();
			return;
		}
	}
}

bool g3_draw_polygon_model(void *model_ptr,grs_bitmap **bitmaps,vms_angvec *angles,fix light,fix *glows)
{
	model_bitmaps = bitmaps;
	anim_angles = angles;
	model_light = light;
	glow_values = glows;
	glow_num = -1;

	interp((ubyte *)model_ptr);
	return 1;
}

static void morph_interp(ubyte *p)
{
	int nv, i, ntris;
	fix light;
	g3s_point *tri[3];
	g3s_uvl tri_uvls[3];
	g3s_uvl *uvl_list;
	ubyte *verts;

	for (;;) {
		switch (W(p)) {

		case OP_EOF:
			return;

		// morphing points live in their own array, always indexed from zero
		case OP_DEFPOINTS:
			nv = W(p+2);
			rotate_point_list(Interp_point_list, morph_points, nv);
			p += 4 + nv * sizeof(vms_vector);
			break;

		case OP_DEFP_START:
			nv = W(p+2);
			rotate_point_list(&Interp_point_list[W(p+4)], morph_points, nv);
			p += 8 + nv * sizeof(vms_vector);
			break;

		case OP_FLATPOLY:
			nv = W(p+2);
			gr_setcolor(W(p+28));
			verts = p + 30;
			tri[0] = &Interp_point_list[W(verts)];
			tri[1] = &Interp_point_list[W(verts+2)];
			tri[2] = &Interp_point_list[W(verts+4)];
			if (nv == 3)
				g3_check_and_draw_poly(3, tri, NULL, NULL);
			else {
				verts += 6;
				for (ntris = nv - 2; ntris > 0; ntris--) {
					g3_check_and_draw_poly(3, tri, NULL, NULL);
					tri[1] = tri[2];
					tri[2] = &Interp_point_list[W(verts)];
					verts += 2;
				}
			}
			p += 30 + vert_list_size(nv);
			break;

		case OP_TMAPPOLY:
			nv = W(p+2);
			uvl_list = UVL(p + 30 + vert_list_size(nv));

			light = surface_light(p+16);
			if (light < 0)
				light = 0;

			tri_uvls[0].l = tri_uvls[1].l = tri_uvls[2].l = light;

			verts = p + 30;
			tri[0] = &Interp_point_list[W(verts)];
			tri[1] = &Interp_point_list[W(verts+2)];
			tri[2] = &Interp_point_list[W(verts+4)];

			if (nv == 3) {
				for (i = 0; i < 3; i++)
					uvl_list[i].l = light;
				g3_check_and_draw_tmap(3, tri, uvl_list, model_bitmaps[W(p+28)], NULL, NULL);
			} else if (nv > 3) {
				g3s_uvl *uv = uvl_list + 3;

				for (i = 0; i < 3; i++) {
					tri_uvls[i].u = uvl_list[i].u;
					tri_uvls[i].v = uvl_list[i].v;
				}
				verts += 6;
				for (ntris = nv - 2; ntris > 0; ntris--) {
					g3_check_and_draw_tmap(3, tri, tri_uvls, model_bitmaps[W(p+28)], NULL, NULL);
					tri[1] = tri[2];
					tri[2] = &Interp_point_list[W(verts)];
					verts += 2;
					tri_uvls[1].u = tri_uvls[2].u;
					tri_uvls[1].v = tri_uvls[2].v;
					tri_uvls[2].u = uv->u;
					tri_uvls[2].v = uv->v;
					uv++;
				}
			}
			p += 30 + vert_list_size(nv) + nv * sizeof(g3s_uvl);
			break;

		case OP_SORTNORM:
			if (g3_check_normal_facing(VP(p+16), VP(p+4))) {
				morph_interp(p + W(p+30));
				morph_interp(p + W(p+28));
			} else {
				morph_interp(p + W(p+28));
				morph_interp(p + W(p+30));
			}
			p += 32;
			break;

		case OP_RODBM: {
			g3s_point rod_bot_p, rod_top_p;

			g3_rotate_point(&rod_bot_p, VP(p+20));
			g3_rotate_point(&rod_top_p, VP(p+4));
			g3_draw_rod_tmap(model_bitmaps[W(p+2)], &rod_bot_p, FX(p+16), &rod_top_p, FX(p+32), f1_0);
			p += 36;
			break;
		}

		case OP_SUBCALL: {
			vms_angvec *a = anim_angles ? &anim_angles[W(p+2)] : &zero_angles;

			g3_start_instance_angles(VP(p+4), a);
			morph_interp(p + W(p+16));
			g3_done_instance();
			p += 20;
			break;
		}

		case OP_GLOW:
			if (glow_values)
				glow_num = W(p+2);
			p += 4;
			break;

		default:
			Int3();
			return;
		}
	}
}

bool g3_draw_morphing_model(void *model_ptr,grs_bitmap **bitmaps,vms_angvec *angles,fix light,vms_vector *new_points)
{
	model_bitmaps = bitmaps;
	anim_angles = angles;
	morph_points = new_points;
	model_light = light;

	morph_interp((ubyte *)model_ptr);
	return 1;
}

static void init_model(ubyte *p)
{
	int nv;

	for (;;) {
		switch (W(p)) {

		case OP_EOF:
			return;

		case OP_DEFPOINTS:
			p += 4 + W(p+2) * sizeof(vms_vector);
			break;

		case OP_DEFP_START:
			p += 8 + W(p+2) * sizeof(vms_vector);
			break;

		case OP_FLATPOLY:
			nv = W(p+2);
			Assert(nv >= 3);
			W(p+28) = gr_find_closest_color_15bpp(W(p+28));
			p += 30 + vert_list_size(nv);
			break;

		case OP_TMAPPOLY:
			nv = W(p+2);
			Assert(nv >= 3);
			if (SW(p+28) > highest_texture_num)
				highest_texture_num = SW(p+28);
			p += 30 + vert_list_size(nv) + nv * sizeof(g3s_uvl);
			break;

		case OP_SORTNORM:
			init_model(p + W(p+28));
			init_model(p + W(p+30));
			p += 32;
			break;

		case OP_RODBM:
			p += 36;
			break;

		case OP_SUBCALL:
			init_model(p + W(p+16));
			p += 20;
			break;

		case OP_GLOW:
			p += 4;
			break;

		default:
			Int3();
			return;
		}
	}
}

void g3_init_polygon_model(void *model_ptr)
{
	highest_texture_num = -1;
	init_model((ubyte *)model_ptr);
}
