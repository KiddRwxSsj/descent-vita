#include <stdlib.h>

#include "globvars.h"
#include "div0.h"

extern void init_interface_vars_to_assembler(void);

void g3_close(void)
{
}

void g3_init(void)
{
	div0_init(DM_ERROR);
	atexit(g3_close);
}

void g3_start_frame(void)
{
	fix s;

	Canvas_width = grd_curcanv->cv_bitmap.bm_w;
	Canvas_height = grd_curcanv->cv_bitmap.bm_h;
	Canv_w2 = Canvas_width << 15;
	Canv_h2 = Canvas_height << 15;

	s = fixmuldiv(grd_curscreen->sc_aspect, Canvas_height, Canvas_width);
	if (s > f1_0) {
		Window_scale.y = fixdiv(f1_0, s);
		Window_scale.x = f1_0;
	} else {
		Window_scale.x = s;
		Window_scale.y = f1_0;
	}
	Window_scale.z = f1_0;

	init_interface_vars_to_assembler();
}

void g3_end_frame(void)
{
}
