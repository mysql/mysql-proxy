/* Copyright (C) 2007, 2008 MySQL AB
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */ 

#include <glib.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>



static int lua_g_usleep (lua_State *L) {
	int ms = luaL_checkinteger (L, 1);

	g_usleep(ms);

	return 0;
}

/*
** Assumes the table is on top of the stack.
*/
static void set_info (lua_State *L) {
	lua_pushliteral (L, "_COPYRIGHT");
	lua_pushliteral (L, "Copyright (C) 2007, 2008 MySQL AB");
	lua_settable (L, -3);
	lua_pushliteral (L, "_DESCRIPTION");
	lua_pushliteral (L, "export glib2-functions as glib.*");
	lua_settable (L, -3);
	lua_pushliteral (L, "_VERSION");
	lua_pushliteral (L, "LuaGlib2 0.1");
	lua_settable (L, -3);
}


static const struct luaL_reg gliblib[] = {
	{"usleep", lua_g_usleep},
	{NULL, NULL},
};

int luaopen_glib2 (lua_State *L) {
	luaL_openlib (L, "glib2", gliblib, 0);
	set_info (L);
	return 1;
}
