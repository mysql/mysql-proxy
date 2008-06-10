#ifndef __LUA_ENV_H__
#define __LUA_ENV_H__

#include <sys/types.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "network-exports.h"

NETWORK_API void lua_getfield_literal (lua_State *L, int idx, const char *k, size_t k_len);
NETWORK_API void *luaL_checkself (lua_State *L);
NETWORK_API int proxy_getmetatable(lua_State *L, const luaL_reg *methods);

#endif
