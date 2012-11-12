/* $%BEGINLICENSE%$
 Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; version 2 of the
 License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA

 $%ENDLICENSE%$ */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <stdio.h>

int main(int argc, char *argv[]) {
	lua_State *L;
	int i;

	if (argc < 2) return -1;

	L = luaL_newstate();
	luaL_openlibs(L);

	lua_newtable(L);
	for (i = 0; i + 1 < argc; i++) {
		lua_pushstring(L, argv[i + 1]);
		lua_rawseti(L, -2, i);
	}
	lua_setglobal(L, "arg");

	if (luaL_dofile(L, argv[1])) {
		fprintf(stderr, "%s: %s", argv[0], lua_tostring(L, -1));
		return -1;
	}


	lua_close(L);
	return 0;
}
