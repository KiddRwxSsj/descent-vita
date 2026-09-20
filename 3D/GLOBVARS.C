#include "globvars.h"

vms_vector View_position;
fix View_zoom;
vms_matrix Unscaled_matrix;
vms_matrix View_matrix;
vms_vector Window_scale;
vms_vector Matrix_scale;

int Canvas_width, Canvas_height;
fix Canv_w2, Canv_h2;

g3s_point *Vbuf0[MAX_POINTS_IN_POLY];
g3s_point *Vbuf1[MAX_POINTS_IN_POLY];
fix Vertex_list[MAX_POINTS_IN_POLY*2];
