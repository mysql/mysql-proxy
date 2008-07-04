/* Copyright (C) 2007, 2008 MySQL AB */ 

/**
 * expose the chassis functions into the lua space
 */


#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "chassis-mainloop.h"

static int lua_chassis_set_shutdown (lua_State *L) {
	chassis_set_shutdown();

	return 0;
}

/*
** Assumes the table is on top of the stack.
*/
static void set_info (lua_State *L) {
	lua_pushliteral (L, "_COPYRIGHT");
	lua_pushliteral (L, "Copyright (C) 2008 MySQL AB");
	lua_settable (L, -3);
	lua_pushliteral (L, "_DESCRIPTION");
	lua_pushliteral (L, "export chassis-functions as chassis.*");
	lua_settable (L, -3);
	lua_pushliteral (L, "_VERSION");
	lua_pushliteral (L, "LuaChassis 0.1");
	lua_settable (L, -3);
}


static const struct luaL_reg chassislib[] = {
	{"set_shutdown", lua_chassis_set_shutdown},
	{NULL, NULL},
};

#if defined(_WIN32)
# define LUAEXT_API __declspec(dllexport)
#else
# define LUAEXT_API extern
#endif

LUAEXT_API int luaopen_chassis (lua_State *L) {
	luaL_register (L, "chassis", chassislib);
	set_info (L);
	return 1;
}
