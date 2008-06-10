#ifndef __NETWORK_CONN_POOL_LUA_H__
#define __NETWORK_CONN_POOL_LUA_H__

#include <lua.h>

#include "network-socket.h"
#include "network-mysqld.h"

#include "network-exports.h"

NETWORK_API int network_connection_pool_getmetatable(lua_State *L);

NETWORK_API int network_connection_pool_lua_add_connection(network_mysqld_con *con);
NETWORK_API network_socket *network_connection_pool_lua_swap(network_mysqld_con *con, int backend_ndx);

#endif
