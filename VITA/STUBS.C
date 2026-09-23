#include <string.h>

#include "types.h"
#include "ipx.h"

// No IPX driver on this hardware; these mirror what the DOS build saw
// when IPX wasn't loaded, so the network menus just report it missing.

int ipx_init( int socket_number, int show_address )
{
	return 3;
}

void ipx_close()
{
}

int ipx_change_default_socket( ushort socket_number )
{
	return -3;
}

ubyte * ipx_get_my_local_address()
{
	static ubyte addr[6];
	return addr;
}

ubyte * ipx_get_my_server_address()
{
	static ubyte addr[4];
	return addr;
}

void ipx_get_local_target( ubyte * server, ubyte * node, ubyte * local_target )
{
	memset(local_target, 0, 6);
}

int ipx_get_packet_data( ubyte * data )
{
	return 0;
}

void ipx_send_broadcast_packet_data( ubyte * data, int datasize )
{
}

void ipx_send_packet_data( ubyte * data, int datasize, ubyte *network, ubyte *address, ubyte *immediate_address )
{
}

void ipx_send_internetwork_packet_data( ubyte * data, int datasize, ubyte * server, ubyte *address )
{
}

void ipx_read_user_file(char * filename)
{
}

void ipx_read_network_file(char * filename)
{
}
