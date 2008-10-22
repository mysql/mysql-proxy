/* Copyright (C) 2008 MySQL AB */ 

#ifndef _QUERY_HANDLING_LUA_H_
#define _QUERY_HANDLING_LUA_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/**
 * embedded lua support
 */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "network-exports.h"
#include "network-injection.h"

NETWORK_API void proxy_getqueuemetatable(lua_State *L);
NETWORK_API void proxy_getinjectionmetatable(lua_State *L);

#endif /* _QUERY_HANDLING_H_ */
