/* $%BEGINLICENSE%$
 Copyright (c) 2008, 2014, Oracle and/or its affiliates. All rights reserved.

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
 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef _WIN32
#include <io.h> /* open, close, ...*/
#endif

#include <glib.h>
#include <glib/gstdio.h>

#include <errno.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA_H
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#include "lua-scope.h"
#include "lua-load-factory.h"

#if GLIB_CHECK_VERSION(2, 16, 0)
#define C(x) x, sizeof(x) - 1

#define START_TEST(x) void (x)(void)
#define END_TEST
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
	lua_scope *sc = lua_scope_new();
	g_assert(sc->L != NULL);

	/* lua_scope_load_script used to give a bus error, when supplying a non-existant script */
	lua_scope_load_script(sc, "/this/is/not/there.lua");
	g_assert(lua_isstring(sc->L, -1));		/* if it's a string, loading failed. exactly what we expect */
	lua_pop(sc->L, 1);
	lua_scope_free(sc);
#else
	g_assert(1 != 0);	/* always succeeds */
#endif
} END_TEST

/**
 * @test luaL_loadfile_factory() errors
 *
 */
START_TEST(test_luaL_loadfile_factory_errors) {
#ifdef HAVE_LUA_H
	lua_State *L = luaL_newstate();
	gchar *tmp_file;
	const gchar *tmp_dir;
	int fd;

	tmp_dir = g_get_tmp_dir();

	/* luaL_loadfile_factory returns LUA_ERRFILE if we use a directory and fopen sets as error EISDIR */
	g_assert_cmpint(LUA_ERRFILE, ==, luaL_loadfile_factory(L, tmp_dir));

	g_assert(lua_isstring(L, -1));
	g_assert_cmpstr(g_strerror(EISDIR), == , lua_tostring(L, -1));

	/* luaL_loadfile_factory returns LUA_ERRFILE if we don't set a path and fopen sets as error EFAULT */
	g_assert_cmpint(LUA_ERRFILE, ==, luaL_loadfile_factory(L, NULL));

	g_assert(lua_isstring(L, -1));
	g_assert_cmpstr(g_strerror(EFAULT), == , lua_tostring(L, -1));

	/* luaL_loadfile_factory returns LUA_ERRFILE if we don't set a path and fopen sets as error ENOENT */
	g_assert_cmpint(LUA_ERRFILE, ==, luaL_loadfile_factory(L, "non-existant-file"));

	g_assert(lua_isstring(L, -1));
	g_assert_cmpstr(g_strerror(ENOENT), == , lua_tostring(L, -1));

	/* luaL_loadfile_factory returns 0 if it succeeded */

	fd = g_file_open_tmp("TestFile-XXXXXX", &tmp_file, NULL);

	g_assert_cmpint(0, ==, luaL_loadfile_factory(L, tmp_file));

	close(fd);
	g_unlink(tmp_file);
	g_free(tmp_file);
	lua_close(L);
#endif
} END_TEST


/*@}*/

int main(int argc, char **argv) {
#ifdef HAVE_GTHREAD	
	g_thread_init(NULL);
#endif

	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/lua-load-factory", test_luaL_loadfile_factory);
	g_test_add_func("/core/lua-loadfile-factory-dir", test_luaL_loadfile_factory_errors);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
