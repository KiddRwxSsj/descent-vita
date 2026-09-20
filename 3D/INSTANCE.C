#include "globvars.h"
#include "error.h"

#define MAX_INSTANCE_DEPTH	5

static struct instance_context {
	vms_matrix m;
	vms_vector p;
} instance_stack[MAX_INSTANCE_DEPTH];

static int instance_depth = 0;

void g3_start_instance_matrix(vms_vector *pos,vms_matrix *orient)
{
	vms_vector tempv;
	vms_matrix tempm, tempm2;

	Assert(instance_depth < MAX_INSTANCE_DEPTH);

	instance_stack[instance_depth].m = View_matrix;
	instance_stack[instance_depth].p = View_position;
	instance_depth++;

	vm_vec_sub2(&View_position, pos);

	if (orient) {
		vm_vec_rotate(&tempv, &View_position, orient);
		View_position = tempv;

		vm_copy_transpose_matrix(&tempm2, orient);
		vm_matrix_x_matrix(&tempm, &tempm2, &View_matrix);
		View_matrix = tempm;
	}
}

void g3_start_instance_angles(vms_vector *pos,vms_angvec *angles)
{
	vms_matrix instmat;

	if (angles == NULL) {
		g3_start_instance_matrix(pos, NULL);
		return;
	}

	vm_angles_2_matrix(&instmat, angles);
	g3_start_instance_matrix(pos, &instmat);
}

void g3_done_instance()
{
	instance_depth--;

	Assert(instance_depth >= 0);

	View_position = instance_stack[instance_depth].p;
	View_matrix = instance_stack[instance_depth].m;
}
