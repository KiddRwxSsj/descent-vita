#include "div0.h"

// Division overflow can't trap on ARM, fixdiv() saturates instead, so the
// exception hooks have nothing to install.

int div0_num_handled_by_cblist = 0;
int div0_num_handled_by_satlist = 0;
int div0_num_saturated = 0;

int div0_init(int mode)
{
	return 1;
}

void div0_close()
{
}

void div0_set_mode(int mode)
{
}

int div0_set_handler( void *div_addr, void *handler_addr )
{
	return 1;
}

int div0_set_saturate( void *div_addr )
{
	return 1;
}
