#ifndef _LUA_LOAD_FACTORY_
#define _LUA_LOAD_FACTORY_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA_H
#include <lua.h>

int luaL_loadstring_factory(lua_State *L, const char *s);
int luaL_loadfile_factory(lua_State *L, const char *filename);
#endif

#endif
