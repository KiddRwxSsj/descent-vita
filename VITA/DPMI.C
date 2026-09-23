#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "dpmi.h"

unsigned int dpmi_virtual_memory = 0;
unsigned int dpmi_available_memory = 128*1024*1024;
unsigned int dpmi_physical_memory = 128*1024*1024;
unsigned int dpmi_dos_memory = 640*1024;

static ubyte low_buffer[1024];

int dpmi_init(int verbose)
{
	return 1;
}

void *dpmi_get_temp_low_buffer( int size )
{
	return (size <= sizeof(low_buffer)) ? low_buffer : NULL;
}

void *dpmi_real_malloc( int size, ushort *selector )
{
	*selector = 1;
	return malloc(size);
}

void dpmi_real_free( ushort selector )
{
}

void dpmi_real_int386x( ubyte intno, dpmi_real_regs * rregs )
{
}

void dpmi_real_call(dpmi_real_regs * rregs)
{
}

int dpmi_lock_region(void *address, unsigned length)
{
	return 1;
}

int dpmi_unlock_region(void *address, unsigned length)
{
	return 1;
}

int dpmi_allocate_selector( void * address, int size, ushort * selector )
{
	*selector = 1;
	return 1;
}

int dpmi_modify_selector_base( ushort selector, void * address )
{
	return 1;
}

int dpmi_modify_selector_limit( ushort selector, int size  )
{
	return 1;
}

int dpmi_set_pm_handler(unsigned intnum, void far * isr )
{
	return 1;
}
