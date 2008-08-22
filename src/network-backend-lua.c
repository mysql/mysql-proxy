#include <lua.h>

#include "lua-env.h"
#include "glib-ext.h"

#define C(x) x, sizeof(x) - 1
#define S(x) x->str, x->len

#include "network-backend.h"
#include "network-mysqld.h"
#include "network-conn-pool-lua.h"
#include "network-backend-lua.h"
#include "network-mysqld-lua.h"

/**
 * get the info about a backend
 *
 * proxy.backend[0].
 *   connected_clients => clients using this backend
 *   address           => ip:port or unix-path of to the backend
 *   state             => int(BACKEND_STATE_UP|BACKEND_STATE_DOWN) 
 *   type              => int(BACKEND_TYPE_RW|BACKEND_TYPE_RO) 
 *
 * @return nil or requested information
 * @see backend_state_t backend_type_t
 */
static int proxy_backend_get(lua_State *L) {
	backend_t *backend = *(backend_t **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	if (strleq(key, keysize, C("connected_clients"))) {
		lua_pushinteger(L, backend->connected_clients);
	} else if (strleq(key, keysize, C("address"))) {
		lua_pushstring(L, backend->addr.str);
	} else if (strleq(key, keysize, C("state"))) {
		lua_pushinteger(L, backend->state);
	} else if (strleq(key, keysize, C("type"))) {
		lua_pushinteger(L, backend->type);
	} else if (strleq(key, keysize, C("uuid"))) {
		if (backend->uuid->len) {
			lua_pushlstring(L, S(backend->uuid));
		} else {
			lua_pushnil(L);
		}
	} else if (strleq(key, keysize, C("pool"))) {
		network_connection_pool *pool; 
		network_connection_pool **pool_p;

		pool_p = lua_newuserdata(L, sizeof(pool)); 
		*pool_p = backend->pool;

		network_connection_pool_getmetatable(L);
		lua_setmetatable(L, -2);
	} else {
		lua_pushnil(L);
	}

	return 1;
}

static int proxy_backend_set(lua_State *L) {
	backend_t *backend = *(backend_t **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	if (strleq(key, keysize, C("state"))) {
		backend->state = lua_tointeger(L, -1);
	} else if (strleq(key, keysize, C("uuid"))) {
		if (lua_isstring(L, -1)) {
			size_t s_len = 0;
			const char *s = lua_tolstring(L, -1, &s_len);

			g_string_assign_len(backend->uuid, s, s_len);
		} else if (lua_isnil(L, -1)) {
			g_string_truncate(backend->uuid, 0);
		} else {
			return luaL_error(L, "proxy.global.backends[...].%s has to be a string", key);
		}
	} else {
		return luaL_error(L, "proxy.global.backends[...].%s is not writable", key);
	}
	return 1;
}

int network_backend_lua_getmetatable(lua_State *L) {
	static const struct luaL_reg methods[] = {
		{ "__index", proxy_backend_get },
		{ "__newindex", proxy_backend_set },
		{ NULL, NULL },
	};

	return proxy_getmetatable(L, methods);
}

/**
 * get proxy.global.backends[ndx]
 *
 * get the backend from the array of mysql backends.
 *
 * @return nil or the backend
 * @see proxy_backend_get
 */
static int proxy_backends_get(lua_State *L) {
	backend_t *backend; 
	backend_t **backend_p;

	GPtrArray *backend_pool = *(GPtrArray **)luaL_checkself(L);
	int backend_ndx = luaL_checkinteger(L, 2) - 1; /** lua is indexes from 1, C from 0 */
	
	/* check that we are in range for a _int_ */
	if (backend_pool->len >= G_MAXINT) {
		return 0;
	}

	if (backend_ndx < 0 ||
	    backend_ndx >= (int)backend_pool->len) {
		lua_pushnil(L);

		return 1;
	}

	backend = backend_pool->pdata[backend_ndx];

	backend_p = lua_newuserdata(L, sizeof(backend)); /* the table underneath proxy.global.backends[ndx] */
	*backend_p = backend;

	network_backend_lua_getmetatable(L);
	lua_setmetatable(L, -2);

	return 1;
}

static int proxy_backends_len(lua_State *L) {
	GPtrArray *backend_pool = *(GPtrArray **)luaL_checkself(L);

	lua_pushinteger(L, backend_pool->len);

	return 1;
}

int network_backends_lua_getmetatable(lua_State *L) {
	static const struct luaL_reg methods[] = {
		{ "__index", proxy_backends_get },
		{ "__len", proxy_backends_len },
		{ NULL, NULL },
	};

	return proxy_getmetatable(L, methods);
}

