#ifndef _LUA_LOAD_FACTORY_
#define _LUA_LOAD_FACTORY_

#include <lua.h>

int luaL_loadstring_factory(lua_State *L, const char *s);
int luaL_loadfile_factory(lua_State *L, const char *filename);

#endif
