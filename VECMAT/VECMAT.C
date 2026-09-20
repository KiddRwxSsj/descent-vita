#include <stdlib.h>

#include "vecmat.h"

#define M1(m) (m)->rvec.x
#define M4(m) (m)->rvec.y
#define M7(m) (m)->rvec.z
#define M2(m) (m)->uvec.x
#define M5(m) (m)->uvec.y
#define M8(m) (m)->uvec.z
#define M3(m) (m)->fvec.x
#define M6(m) (m)->fvec.y
#define M9(m) (m)->fvec.z

vms_vector vmd_zero_vector = {0,0,0};
vms_matrix vmd_identity_matrix = { {f1_0,0,0}, {0,f1_0,0}, {0,0,f1_0} };

typedef long long qint;

static inline fix qfix(qint q)
{
	return (fix)(q >> 16);
}

static inline fix vec_mag_sq(fix x,fix y,fix z)
{
	qint q = (qint)x*x + (qint)y*y + (qint)z*z;

	return quad_sqrt((long)(uint)q, (long)(q >> 32));
}

vms_vector *vm_vec_make(vms_vector *v,fix x,fix y,fix z)
{
	v->x = x; v->y = y; v->z = z;
	return v;
}

vms_angvec *vm_angvec_make(vms_angvec *v,fixang p,fixang b,fixang h)
{
	v->p = p; v->b = b; v->h = h;
	return v;
}

vms_vector *vm_vec_add(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	dest->x = src0->x + src1->x;
	dest->y = src0->y + src1->y;
	dest->z = src0->z + src1->z;
	return dest;
}

vms_vector *vm_vec_sub(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	dest->x = src0->x - src1->x;
	dest->y = src0->y - src1->y;
	dest->z = src0->z - src1->z;
	return dest;
}

vms_vector *vm_vec_add2(vms_vector *dest,vms_vector *src)
{
	dest->x += src->x;
	dest->y += src->y;
	dest->z += src->z;
	return dest;
}

vms_vector *vm_vec_sub2(vms_vector *dest,vms_vector *src)
{
	dest->x -= src->x;
	dest->y -= src->y;
	dest->z -= src->z;
	return dest;
}

vms_vector *vm_vec_avg(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	dest->x = (src0->x + src1->x) >> 1;
	dest->y = (src0->y + src1->y) >> 1;
	dest->z = (src0->z + src1->z) >> 1;
	return dest;
}

vms_vector *vm_vec_avg4(vms_vector *dest,vms_vector *src0,vms_vector *src1,vms_vector *src2,vms_vector *src3)
{
	dest->x = (src0->x + src1->x + src2->x + src3->x) >> 2;
	dest->y = (src0->y + src1->y + src2->y + src3->y) >> 2;
	dest->z = (src0->z + src1->z + src2->z + src3->z) >> 2;
	return dest;
}

vms_vector *vm_vec_scale(vms_vector *dest,fix s)
{
	dest->x = fixmul(dest->x, s);
	dest->y = fixmul(dest->y, s);
	dest->z = fixmul(dest->z, s);
	return dest;
}

vms_vector *vm_vec_copy_scale(vms_vector *dest,vms_vector *src,fix s)
{
	dest->x = fixmul(src->x, s);
	dest->y = fixmul(src->y, s);
	dest->z = fixmul(src->z, s);
	return dest;
}

vms_vector *vm_vec_scale_add(vms_vector *dest,vms_vector *src1,vms_vector *src2,fix k)
{
	dest->x = src1->x + fixmul(src2->x, k);
	dest->y = src1->y + fixmul(src2->y, k);
	dest->z = src1->z + fixmul(src2->z, k);
	return dest;
}

vms_vector *vm_vec_scale_add2(vms_vector *dest,vms_vector *src,fix k)
{
	dest->x += fixmul(src->x, k);
	dest->y += fixmul(src->y, k);
	dest->z += fixmul(src->z, k);
	return dest;
}

vms_vector *vm_vec_scale2(vms_vector *dest,fix n,fix d)
{
	if (d == 0)
		return dest;
	dest->x = fixmuldiv(dest->x, n, d);
	dest->y = fixmuldiv(dest->y, n, d);
	dest->z = fixmuldiv(dest->z, n, d);
	return dest;
}

fix vm_vec_mag(vms_vector *v)
{
	return vec_mag_sq(v->x, v->y, v->z);
}

fix vm_vec_dist(vms_vector *v0,vms_vector *v1)
{
	return vec_mag_sq(v0->x - v1->x, v0->y - v1->y, v0->z - v1->z);
}

static fix mag_quick(fix a,fix b,fix c)
{
	fix t;

	if (!(a > b)) { t = a; a = b; b = t; }
	if (!(b > c)) {
		t = b; b = c; c = t;
		if (!(a > b)) { t = a; a = b; b = t; }
	}
	b >>= 2;
	c >>= 3;
	b += c;
	a += b;
	b >>= 1;
	return a + b;
}

fix vm_vec_mag_quick(vms_vector *v)
{
	return mag_quick(labs(v->x), labs(v->y), labs(v->z));
}

fix vm_vec_dist_quick(vms_vector *v0,vms_vector *v1)
{
	return mag_quick(labs(v0->x - v1->x), labs(v0->y - v1->y), labs(v0->z - v1->z));
}

fix vm_vec_copy_normalize(vms_vector *dest,vms_vector *src)
{
	fix m = vm_vec_mag(src);

	if (m) {
		dest->x = fixdiv(src->x, m);
		dest->y = fixdiv(src->y, m);
		dest->z = fixdiv(src->z, m);
	}
	return m;
}

fix vm_vec_normalize(vms_vector *v)
{
	return vm_vec_copy_normalize(v, v);
}

fix vm_vec_copy_normalize_quick(vms_vector *dest,vms_vector *src)
{
	fix m = vm_vec_mag_quick(src);

	if (m) {
		dest->x = fixdiv(src->x, m);
		dest->y = fixdiv(src->y, m);
		dest->z = fixdiv(src->z, m);
	}
	return m;
}

fix vm_vec_normalize_quick(vms_vector *v)
{
	return vm_vec_copy_normalize_quick(v, v);
}

fix vm_vec_normalized_dir(vms_vector *dest,vms_vector *end,vms_vector *start)
{
	vm_vec_sub(dest, end, start);
	return vm_vec_normalize(dest);
}

fix vm_vec_normalized_dir_quick(vms_vector *dest,vms_vector *end,vms_vector *start)
{
	vm_vec_sub(dest, end, start);
	return vm_vec_normalize_quick(dest);
}

// Returns a saturated value if the 64-bit sum doesn't fit in 16.32,
// exactly like the overflow check the asm version always did.
fix vm_vec_dotprod(vms_vector *v0,vms_vector *v1)
{
	qint q = (qint)v0->x * v1->x + (qint)v0->y * v1->y + (qint)v0->z * v1->z;
	qint top = q >> 47;

	if (top != 0 && top != -1)
		return (q < 0) ? -0x7fffffff : 0x7fffffff;
	return qfix(q);
}

fix vm_vec_dot(vms_vector *v0,vms_vector *v1)
{
	return vm_vec_dotprod(v0, v1);
}

vms_vector *vm_vec_crossprod(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	dest->x = qfix((qint)src0->y * src1->z - (qint)src0->z * src1->y);
	dest->y = qfix((qint)src0->z * src1->x - (qint)src0->x * src1->z);
	dest->z = qfix((qint)src0->x * src1->y - (qint)src0->y * src1->x);
	return dest;
}

vms_vector *vm_vec_cross(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	return vm_vec_crossprod(dest, src0, src1);
}

// Shift the vector so its largest component sits roughly in 15..18 bits,
// which keeps the following cross product from over/underflowing.
static void check_vec(vms_vector *v)
{
	fix m;
	int shift = 0;

	m = labs(v->x) | labs(v->y) | labs(v->z);
	if (m == 0)
		return;

	if (m & 0xfffc0000) {
		while (m & 0x00f00000) { shift += 4; m >>= 4; }
		while (m & 0xfffc0000) { shift += 2; m >>= 2; }
		v->x >>= shift;
		v->y >>= shift;
		v->z >>= shift;
	} else if (!(m & 0xffff8000)) {
		while (!(m & 0xfffff000)) { shift += 4; m <<= 4; }
		while (!(m & 0xffff8000)) { shift += 2; m <<= 2; }
		v->x <<= shift;
		v->y <<= shift;
		v->z <<= shift;
	}
}

vms_vector *vm_vec_perp(vms_vector *dest,vms_vector *p0,vms_vector *p1,vms_vector *p2)
{
	vms_vector t0, t1;

	vm_vec_sub(&t1, p2, p1);
	vm_vec_sub(&t0, p1, p0);
	check_vec(&t0);
	check_vec(&t1);
	return vm_vec_crossprod(dest, &t0, &t1);
}

vms_vector *vm_vec_normal(vms_vector *dest,vms_vector *p0,vms_vector *p1,vms_vector *p2)
{
	vm_vec_perp(dest, p0, p1, p2);
	vm_vec_normalize(dest);
	return dest;
}

static vms_matrix *sincos_2_matrix(vms_matrix *m,fix sinp,fix cosp,fix sinb,fix cosb,fix sinh,fix cosh)
{
	fix sbsh, cbch, cbsh, sbch;

	sbsh = fixmul(sinb, sinh);
	cbch = fixmul(cosb, cosh);
	M1(m) = cbch + fixmul(sbsh, sinp);
	M8(m) = fixmul(cbch, sinp) + sbsh;

	cbsh = fixmul(cosb, sinh);
	sbch = fixmul(sinb, cosh);
	M2(m) = fixmul(cbsh, sinp) - sbch;
	M7(m) = fixmul(sbch, sinp) - cbsh;

	M3(m) = fixmul(sinh, cosp);
	M4(m) = fixmul(sinb, cosp);
	M5(m) = fixmul(cosb, cosp);
	M6(m) = -sinp;
	M9(m) = fixmul(cosh, cosp);

	return m;
}

vms_matrix *vm_angles_2_matrix(vms_matrix *m,vms_angvec *a)
{
	fix sinp, cosp, sinb, cosb, sinh, cosh;

	fix_sincos(a->p, &sinp, &cosp);
	fix_sincos(a->b, &sinb, &cosb);
	fix_sincos(a->h, &sinh, &cosh);
	return sincos_2_matrix(m, sinp, cosp, sinb, cosb, sinh, cosh);
}

vms_matrix *vm_vec_ang_2_matrix(vms_matrix *m,vms_vector *v,fixang a)
{
	fix sinp, cosp, sinb, cosb, sinh, cosh;

	fix_sincos(a, &sinb, &cosb);
	sinp = -v->y;
	cosp = fix_sqrt(f1_0 - fixmul(sinp, sinp));
	sinh = fixdiv(v->x, cosp);
	cosh = fixdiv(v->z, cosp);
	return sincos_2_matrix(m, sinp, cosp, sinb, cosb, sinh, cosh);
}

vms_matrix *vm_vector_2_matrix(vms_matrix *m,vms_vector *fvec,vms_vector *uvec,vms_vector *rvec)
{
	vms_vector xvec, yvec, zvec;

	if (uvec) {
		zvec = *fvec;
		if (vm_vec_normalize(&zvec) == 0) goto bad_vector2;
		yvec = *uvec;
		if (vm_vec_normalize(&yvec) == 0) goto bad_vector2;
		vm_vec_crossprod(&xvec, &yvec, &zvec);
		if (vm_vec_normalize(&xvec) == 0) goto bad_vector2;
		vm_vec_crossprod(&yvec, &zvec, &xvec);
		goto copy_into_matrix;
	}

	if (rvec) {
		zvec = *fvec;
		if (vm_vec_normalize(&zvec) == 0) goto bad_vector2;
		xvec = *rvec;
		if (vm_vec_normalize(&xvec) == 0) goto bad_vector2;
		vm_vec_crossprod(&yvec, &zvec, &xvec);
		if (vm_vec_normalize(&yvec) == 0) goto bad_vector2;
		vm_vec_crossprod(&xvec, &yvec, &zvec);
		goto copy_into_matrix;
	}

	zvec = *fvec;
	goto just_forward_vec;

bad_vector2:
just_forward_vec:
	if (vm_vec_normalize(&zvec) == 0)
		return m;
	m->fvec = zvec;

	if ((zvec.x | zvec.z) == 0) {
		M1(m) = f1_0;
		M8(m) = (zvec.y < 0) ? f1_0 : -f1_0;
		M4(m) = M7(m) = M2(m) = M5(m) = 0;
		return m;
	}

	xvec.x = zvec.z;
	xvec.y = 0;
	xvec.z = -zvec.x;
	vm_vec_normalize(&xvec);
	m->rvec = xvec;
	vm_vec_crossprod(&yvec, &zvec, &xvec);
	m->uvec = yvec;
	return m;

copy_into_matrix:
	m->rvec = xvec;
	m->uvec = yvec;
	m->fvec = zvec;
	return m;
}

vms_matrix *vm_vector_2_matrix_norm(vms_matrix *m,vms_vector *fvec,vms_vector *uvec,vms_vector *rvec)
{
	return vm_vector_2_matrix(m, fvec, uvec, rvec);
}

static inline fix row_dot(fix x0,fix y0,fix z0,fix x1,fix y1,fix z1)
{
	return qfix((qint)x0*x1 + (qint)y0*y1 + (qint)z0*z1);
}

vms_vector *vm_vec_rotate(vms_vector *dest,vms_vector *src,vms_matrix *m)
{
	dest->x = row_dot(src->x, src->y, src->z, M1(m), M4(m), M7(m));
	dest->y = row_dot(src->x, src->y, src->z, M2(m), M5(m), M8(m));
	dest->z = row_dot(src->x, src->y, src->z, M3(m), M6(m), M9(m));
	return dest;
}

vms_matrix *vm_transpose_matrix(vms_matrix *m)
{
	fix t;

	t = M2(m); M2(m) = M4(m); M4(m) = t;
	t = M3(m); M3(m) = M7(m); M7(m) = t;
	t = M6(m); M6(m) = M8(m); M8(m) = t;
	return m;
}

vms_matrix *vm_transpose(vms_matrix *m)
{
	return vm_transpose_matrix(m);
}

vms_matrix *vm_copy_transpose_matrix(vms_matrix *dest,vms_matrix *src)
{
	M1(dest) = M1(src);
	M4(dest) = M2(src);
	M7(dest) = M3(src);
	M2(dest) = M4(src);
	M5(dest) = M5(src);
	M8(dest) = M6(src);
	M3(dest) = M7(src);
	M6(dest) = M8(src);
	M9(dest) = M9(src);
	return dest;
}

vms_matrix *vm_copy_transpose(vms_matrix *dest,vms_matrix *src)
{
	return vm_copy_transpose_matrix(dest, src);
}

vms_matrix *vm_matrix_x_matrix(vms_matrix *dest,vms_matrix *s0,vms_matrix *s1)
{
	M1(dest) = row_dot(M1(s0), M2(s0), M3(s0), M1(s1), M4(s1), M7(s1));
	M2(dest) = row_dot(M1(s0), M2(s0), M3(s0), M2(s1), M5(s1), M8(s1));
	M3(dest) = row_dot(M1(s0), M2(s0), M3(s0), M3(s1), M6(s1), M9(s1));
	M4(dest) = row_dot(M4(s0), M5(s0), M6(s0), M1(s1), M4(s1), M7(s1));
	M5(dest) = row_dot(M4(s0), M5(s0), M6(s0), M2(s1), M5(s1), M8(s1));
	M6(dest) = row_dot(M4(s0), M5(s0), M6(s0), M3(s1), M6(s1), M9(s1));
	M7(dest) = row_dot(M7(s0), M8(s0), M9(s0), M1(s1), M4(s1), M7(s1));
	M8(dest) = row_dot(M7(s0), M8(s0), M9(s0), M2(s1), M5(s1), M8(s1));
	M9(dest) = row_dot(M7(s0), M8(s0), M9(s0), M3(s1), M6(s1), M9(s1));
	return dest;
}

fixang vm_vec_delta_ang_norm(vms_vector *v0,vms_vector *v1,vms_vector *fvec)
{
	vms_vector t;
	fixang a;

	a = fix_acos(vm_vec_dotprod(v0, v1));
	if (fvec) {
		vm_vec_crossprod(&t, v0, v1);
		if (vm_vec_dotprod(&t, fvec) < 0)
			a = -a;
	}
	return a;
}

fixang vm_vec_delta_ang(vms_vector *v0,vms_vector *v1,vms_vector *fvec)
{
	vm_vec_normalize(v0);
	vm_vec_normalize(v1);
	return vm_vec_delta_ang_norm(v0, v1, fvec);
}

fix vm_dist_to_plane(vms_vector *checkp,vms_vector *norm,vms_vector *planep)
{
	vms_vector t;

	vm_vec_sub(&t, checkp, planep);
	return vm_vec_dotprod(&t, norm);
}

vms_angvec *vm_extract_angles_matrix(vms_angvec *a,vms_matrix *m)
{
	fix sinh, cosh, cosp, sinb, cosb;

	if ((m->fvec.x | m->fvec.z) == 0)
		a->h = 0;
	else
		a->h = fix_atan2(m->fvec.z, m->fvec.x);

	fix_sincos(a->h, &sinh, &cosh);

	if (labs(cosh) > labs(sinh))
		cosp = fixdiv(m->fvec.z, cosh);
	else
		cosp = fixdiv(M3(m), sinh);

	if ((cosp | -m->fvec.y) == 0)
		a->p = 0;
	else
		a->p = fix_atan2(cosp, -m->fvec.y);

	if (cosp == 0) {
		a->b = 0;
		return a;
	}

	sinb = fixdiv(M4(m), cosp);
	cosb = fixdiv(M5(m), cosp);
	if ((sinb | cosb) == 0)
		a->b = 0;
	else
		a->b = fix_atan2(cosb, sinb);

	return a;
}

vms_angvec *vm_extract_angles_vector_normalized(vms_angvec *a,vms_vector *v)
{
	a->b = 0;
	a->p = fix_asin(-v->y);
	if ((v->x | v->z) == 0)
		a->h = 0;
	else
		a->h = fix_atan2(v->z, v->x);
	return a;
}

vms_angvec *vm_extract_angles_vector(vms_angvec *a,vms_vector *v)
{
	vms_vector t;

	if (vm_vec_copy_normalize(&t, v) == 0)
		return a;
	return vm_extract_angles_vector_normalized(a, &t);
}
