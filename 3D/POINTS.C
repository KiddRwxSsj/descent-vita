#include "globvars.h"

// Same test the asm divcheck macro does before an idiv: if the quotient
// can't fit in 32 bits the divide would trap, so the caller bails instead.
int g3_div_overflow(long long num, fix den)
{
	int hi = (int)(num >> 32);
	uint c = (hi < 0) ? (uint)-hi : (uint)hi;

	c = (c << 1) | ((uint)num >> 31);
	return c >= (uint)den;
}

ubyte g3_code_point(g3s_point *p)
{
	ubyte cc = 0;

	if (p->x > p->z)
		cc |= CC_OFF_RIGHT;
	if (p->y > p->z)
		cc |= CC_OFF_TOP;
	if (!(-p->z < 0))
		cc |= CC_BEHIND;
	if (p->x < -p->z)
		cc |= CC_OFF_LEFT;
	if (p->y < -p->z)
		cc |= CC_OFF_BOT;

	return p->p3_codes = cc;
}

ubyte g3_rotate_point(g3s_point *dest,vms_vector *src)
{
	vms_vector t;

	vm_vec_sub(&t, src, &View_position);
	vm_vec_rotate(&dest->p3_vec, &t, &View_matrix);
	dest->p3_flags = 0;
	return g3_code_point(dest);
}

void g3_project_point(g3s_point *p)
{
	long long n;

	if ((p->p3_flags & PF_PROJECTED) || (p->p3_codes & CC_BEHIND))
		return;

	n = (long long)p->x * Canv_w2;
	if (g3_div_overflow(n, p->z)) {
		p->p3_flags = PF_OVERFLOW;
		return;
	}
	p->p3_sx = g3_div(n, p->z) + Canv_w2;

	n = (long long)p->y * Canv_h2;
	if (g3_div_overflow(n, p->z)) {
		p->p3_flags = PF_OVERFLOW;
		return;
	}
	p->p3_sy = Canv_h2 - g3_div(n, p->z);

	p->p3_flags |= PF_PROJECTED;
}

void g3_point_2_vec(vms_vector *v,short sx,short sy)
{
	vms_vector t;
	vms_matrix m;

	t.x = fixmuldiv(fixdiv(((fix)sx << 16) - Canv_w2, Canv_w2), Matrix_scale.z, Matrix_scale.x);
	t.y = -fixmuldiv(fixdiv(((fix)sy << 16) - Canv_h2, Canv_h2), Matrix_scale.z, Matrix_scale.y);
	t.z = f1_0;

	vm_vec_normalize(&t);
	vm_copy_transpose_matrix(&m, &Unscaled_matrix);
	vm_vec_rotate(v, &t, &m);
}

vms_vector *g3_rotate_delta_x(vms_vector *dest,fix dx)
{
	dest->x = fixmul(View_matrix.rvec.x, dx);
	dest->y = fixmul(View_matrix.uvec.x, dx);
	dest->z = fixmul(View_matrix.fvec.x, dx);
	return dest;
}

vms_vector *g3_rotate_delta_y(vms_vector *dest,fix dy)
{
	dest->x = fixmul(View_matrix.rvec.y, dy);
	dest->y = fixmul(View_matrix.uvec.y, dy);
	dest->z = fixmul(View_matrix.fvec.y, dy);
	return dest;
}

vms_vector *g3_rotate_delta_z(vms_vector *dest,fix dz)
{
	dest->x = fixmul(View_matrix.rvec.z, dz);
	dest->y = fixmul(View_matrix.uvec.z, dz);
	dest->z = fixmul(View_matrix.fvec.z, dz);
	return dest;
}

vms_vector *g3_rotate_delta_vec(vms_vector *dest,vms_vector *src)
{
	return vm_vec_rotate(dest, src, &View_matrix);
}

ubyte g3_add_delta_vec(g3s_point *dest,g3s_point *src,vms_vector *deltav)
{
	vm_vec_add(&dest->p3_vec, &src->p3_vec, deltav);
	dest->p3_flags = 0;
	return g3_code_point(dest);
}

fix g3_calc_point_depth(vms_vector *pnt)
{
	long long q;

	q = (long long)(pnt->x - View_position.x) * View_matrix.fvec.x;
	q += (long long)(pnt->y - View_position.y) * View_matrix.fvec.y;
	q += (long long)(pnt->z - View_position.z) * View_matrix.fvec.z;
	return (fix)(q >> 16);
}
