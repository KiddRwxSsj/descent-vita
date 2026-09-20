#include "globvars.h"

static void scale_matrix(void)
{
	fix s;

	Unscaled_matrix = View_matrix;
	Matrix_scale = Window_scale;

	if (View_zoom <= f1_0)
		Matrix_scale.z = fixmul(Matrix_scale.z, View_zoom);
	else {
		s = fixdiv(f1_0, View_zoom);
		Matrix_scale.x = fixmul(Matrix_scale.x, s);
		Matrix_scale.y = fixmul(Matrix_scale.y, s);
	}

	vm_vec_scale(&View_matrix.rvec, Matrix_scale.x);
	vm_vec_scale(&View_matrix.uvec, Matrix_scale.y);
	vm_vec_scale(&View_matrix.fvec, Matrix_scale.z);
}

void g3_set_view_angles(vms_vector *view_pos,vms_angvec *view_orient,fix zoom)
{
	View_zoom = zoom;
	View_position = *view_pos;
	vm_angles_2_matrix(&View_matrix, view_orient);
	scale_matrix();
}

void g3_set_view_matrix(vms_vector *view_pos,vms_matrix *view_matrix,fix zoom)
{
	View_zoom = zoom;
	View_position = *view_pos;
	View_matrix = *view_matrix;
	scale_matrix();
}
