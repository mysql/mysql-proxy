#ifndef __NETWORK_SOCKET_LUA_H__
#define __NETWORK_SOCKET_LUA_H__

#include <lua.h>

#include "network-exports.h"

NETWORK_API int network_socket_lua_getmetatable(lua_State *L);

#endif
