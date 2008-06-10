#ifndef __NETWORK_BACKEND_LUA_H__
#define __NETWORK_BACKEND_LUA_H__

#include <lua.h>

#include "network-exports.h"

NETWORK_API int network_backend_lua_getmetatable(lua_State *L);
NETWORK_API int network_backends_lua_getmetatable(lua_State *L);

#endif
