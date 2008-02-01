#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h> /* got g_stat() */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LUA_H
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#endif

#include <mysql.h>
#include <mysqld_error.h> /** for ER_UNKNOWN_ERROR */

#include "lua-load-factory.h"
#include "lua-scope.h"

lua_scope *lua_scope_init(void) {
	lua_scope *sc;

	sc = g_new0(lua_scope, 1);

#ifdef HAVE_LUA_H
	sc->L = luaL_newstate();
	luaL_openlibs(sc->L);
#endif

#ifdef HAVE_GTHREAD
	sc->mutex = g_mutex_new();
#endif

	return sc;
}

void lua_scope_free(lua_scope *sc) {
	if (!sc) return;

#ifdef HAVE_LUA_H
	g_assert(lua_gettop(sc->L) == 0);

	/* FIXME: we might want to cleanup the cached-scripts in the registry */

	lua_close(sc->L);
#endif
#ifdef HAVE_GTHREAD
	g_mutex_free(sc->mutex);
#endif

	g_free(sc);
}

void lua_scope_get(lua_scope *sc) {
#ifdef HAVE_GTHREAD
	g_mutex_lock(sc->mutex);
#endif
#ifdef HAVE_LUA_H
	sc->L_top = lua_gettop(sc->L);
#endif

	return;
}

void lua_scope_release(lua_scope *sc) {
#ifdef HAVE_LUA_H
	if (lua_gettop(sc->L) != sc->L_top) {
		g_critical("%s: lua-stack out of sync: is %d, should be %d", G_STRLOC, lua_gettop(sc->L), sc->L_top);
	}
#endif

#ifdef HAVE_GTHREAD
	g_mutex_unlock(sc->mutex);
#endif
	return;
}

#ifdef HAVE_LUA_H
/**
 * load the lua script
 *
 * wraps luaL_loadfile and prints warnings when needed
 *
 * on success we leave a function on the stack, otherwise a error-msg
 *
 * @see luaL_loadfile
 * @returns the lua_State
 */
lua_State *lua_scope_load_script(lua_scope *sc, const gchar *name) {
	lua_State *L = sc->L;
	int stack_top = lua_gettop(L);
	/**
	 * check if the script is in the cache already
	 *
	 * if it is and is fresh, duplicate it
	 * otherwise load it and put it in the cache
	 */
#if 1
	lua_getfield(L, LUA_REGISTRYINDEX, "cachedscripts");         /* sp += 1 */
	if (lua_isnil(L, -1)) {
		/** oops, not there yet */
		lua_pop(L, 1);                                       /* sp -= 1 */

		lua_newtable(L);             /* reg.cachedscripts = { } sp += 1 */
		lua_setfield(L, LUA_REGISTRYINDEX, "cachedscripts"); /* sp -= 1 */
	
		lua_getfield(L, LUA_REGISTRYINDEX, "cachedscripts"); /* sp += 1 */
	}
	g_assert(lua_istable(L, -1)); /** the script-cache should be on the stack now */

	g_assert(lua_gettop(L) == stack_top + 1);

	/**
	 * reg.
	 *   cachedscripts.  <- on the stack
	 *     <name>.
	 *       mtime
	 *       func
	 */

	lua_getfield(L, -1, name);
	if (lua_istable(L, -1)) {
		struct stat st;
		time_t cached_mtime;
		off_t cached_size;

		/** the script cached, check that it is fresh */
		if (0 != g_stat(name, &st)) {
			gchar *errmsg;
			/* stat() failed, ... not good */

			lua_pop(L, 2); /* cachedscripts. + cachedscripts.<name> */

			errmsg = g_strdup_printf("%s: stat(%s) failed: %s (%d)",
				       G_STRLOC, name, strerror(errno), errno);
			
			lua_pushstring(L, errmsg);

			g_free(errmsg);

			g_assert(lua_isstring(L, -1));
			g_assert(lua_gettop(L) == stack_top + 1);

			return L;
		}

		/* get the mtime from the table */
		lua_getfield(L, -1, "mtime");
		g_assert(lua_isnumber(L, -1));
		cached_mtime = lua_tonumber(L, -1);
		lua_pop(L, 1);

		/* get the mtime from the table */
		lua_getfield(L, -1, "size");
		g_assert(lua_isnumber(L, -1));
		cached_size = lua_tonumber(L, -1);
		lua_pop(L, 1);

		if (st.st_mtime != cached_mtime || 
		    st.st_size  != cached_size) {
			lua_pushnil(L);
			lua_setfield(L, -2, "func"); /* zap the old function on the stack */

			if (0 != luaL_loadfile_factory(L, name)) {
				/* log a warning and leave the error-msg on the stack */
				g_warning("%s: reloading '%s' failed", G_STRLOC, name);

				/* cleanup a bit */
				lua_remove(L, -2); /* remove the cachedscripts.<name> */
				lua_remove(L, -2); /* remove cachedscripts-table */

				g_assert(lua_isstring(L, -1));
				g_assert(lua_gettop(L) == stack_top + 1);

				return L;
			}
			lua_setfield(L, -2, "func");

			/* not fresh, reload */
			lua_pushinteger(L, st.st_mtime);
			lua_setfield(L, -2, "mtime");   /* t.mtime = ... */

			lua_pushinteger(L, st.st_size);
			lua_setfield(L, -2, "size");    /* t.size = ... */
		}
	} else if (lua_isnil(L, -1)) {
		struct stat st;

		lua_pop(L, 1); /* remove the nil, aka not found */

		/** not known yet */
		lua_newtable(L);                /* t = { } */
		
		if (0 != g_stat(name, &st)) {
		}

		if (0 != luaL_loadfile_factory(L, name)) {
			/* log a warning and leave the error-msg on the stack */
			g_warning("luaL_loadfile(%s) failed", name);

			/* cleanup a bit */
			lua_remove(L, -2); /* remove the t = { } */
			lua_remove(L, -2); /* remove cachedscripts-table */

			g_assert(lua_isstring(L, -1));
			g_assert(lua_gettop(L) == stack_top + 1);

			return L;
		}

		lua_setfield(L, -2, "func");

		lua_pushinteger(L, st.st_mtime);
		lua_setfield(L, -2, "mtime");   /* t.mtime = ... */

		lua_pushinteger(L, st.st_size);
		lua_setfield(L, -2, "size");    /* t.size  = ... */

		lua_setfield(L, -2, name);      /* reg.cachedscripts.<name> = t */

		lua_getfield(L, -1, name);
	} else {
		/* not good */
		lua_pushstring(L, "stack is out of sync");

		g_return_val_if_reached(L);
	}

	/* -- the cache is fresh now, get the script from it */

	g_assert(lua_istable(L, -1));
	lua_getfield(L, -1, "func");
	g_assert(lua_isfunction(L, -1));

	/* cachedscripts and <name> are still on the stack */
#if 0
	g_debug("(load) [-3] %s", lua_typename(L, lua_type(L, -3)));
	g_debug("(load) [-2] %s", lua_typename(L, lua_type(L, -2)));
	g_debug("(load) [-1] %s", lua_typename(L, lua_type(L, -1)));
#endif
	lua_remove(L, -2); /* remove the reg.cachedscripts.<name> */
	lua_remove(L, -2); /* remove the reg.cachedscripts */

	/* create a copy of the script for us:
	 *
	 * f = function () 
	 *   return function () 
	 *     <script> 
	 *   end 
	 * end
	 * f()
	 *
	 * */
	if (0 != lua_pcall(L, 0, 1, 0)) {
		g_warning("lua_pcall(factory<%s>) failed", name);

		return L;
	}
#else
	if (0 != luaL_loadfile(L, name)) {
		/* log a warning and leave the error-msg on the stack */
		g_warning("luaL_loadfile(%s) failed", name);

		return L;
	}
#endif
	g_assert(lua_isfunction(L, -1));
	g_assert(lua_gettop(L) == stack_top + 1);

	return L;
}
#endif

