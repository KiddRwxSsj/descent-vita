#include "types.h"
#include "fix.h"
#include "mouse.h"

int mouse_init(int enable_cyberman)
{
	return 0;
}

int mouse_set_limits( int x1, int y1, int x2, int y2 )
{
	return 0;
}

void mouse_flush()
{
}

void mouse_close()
{
}

void mouse_get_pos( int *x, int *y)
{
	*x = *y = 0;
}

void mouse_get_delta( int *dx, int *dy )
{
	*dx = *dy = 0;
}

int mouse_get_btns()
{
	return 0;
}

void mouse_set_pos( int x, int y)
{
}

void mouse_get_cyberman_pos( int *x, int *y )
{
	*x = *y = 0;
}

fix mouse_button_down_time(int button)
{
	return 0;
}

int mouse_button_down_count(int button)
{
	return 0;
}

int mouse_button_state(int button)
{
	return 0;
}
