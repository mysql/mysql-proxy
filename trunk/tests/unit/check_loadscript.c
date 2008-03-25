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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <check.h>

#ifdef HAVE_LUA_H
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#include "lua-scope.h"

#define C(x) x, sizeof(x) - 1

/**
 * Tests for the Lua script loading facility
 * @ingroup Core
 */

/*@{*/

/**
 * @test luaL_loadfile_factory()
 *
 */
START_TEST(test_luaL_loadfile_factory) {
#ifdef HAVE_LUA_H
	lua_scope *sc = lua_scope_init();
	fail_unless(sc->L != NULL);
	
	/* lua_scope_load_script used to give a bus error, when supplying a non-existant script */
	lua_scope_load_script(sc, "/this/is/not/there.lua");
	fail_unless(lua_isstring(sc->L, -1));		/* if it's a string, loading failed. exactly what we expect */
	lua_pop(sc->L, 1);
	lua_scope_free(sc);
#else
	fail_unless(1 != 0);	/* always succeeds */
#endif
} END_TEST


/*@}*/

Suite *loadfile_suite(void) {
	Suite *s = suite_create("lua-loading");
	TCase *tc_core = tcase_create("Core");

	suite_add_tcase (s, tc_core);
	tcase_add_test(tc_core, test_luaL_loadfile_factory);

#ifdef HAVE_GTHREAD	
	g_thread_init(NULL);
#endif

	return s;
}

int main() {
	int nf;
	Suite *s = loadfile_suite();
	SRunner *sr = srunner_create(s);
		
	srunner_run_all(sr, CK_ENV);

	nf = srunner_ntests_failed(sr);

	srunner_free(sr);
	
	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}