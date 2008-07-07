
#include <lua.h>

#include "lua-env.h"
#include "glib-ext.h"

#include "network-socket.h"
#include "network-mysqld-packet.h"
#include "network-socket-lua.h"

#define C(x) x, sizeof(x) - 1
#define S(x) x->str, x->len

static int proxy_socket_get(lua_State *L) {
	network_socket *sock = *(network_socket **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	/**
	 * we to split it in .client and .server here
	 */

	if (strleq(key, keysize, C("default_db"))) {
		lua_pushlstring(L, sock->default_db->str, sock->default_db->len);
		return 1;
	} else if (strleq(key, keysize, C("address"))) {
		lua_pushstring(L, sock->addr.str);
		return 1;
	}
       
	if (sock->response) {
		if (strleq(key, keysize, C("username"))) {
			lua_pushlstring(L, S(sock->response->username));
			return 1;
		} else if (strleq(key, keysize, C("address"))) {
			lua_pushstring(L, sock->addr.str);
			return 1;
		} else if (strleq(key, keysize, C("scrambled_password"))) {
			lua_pushlstring(L, S(sock->response->response));
			return 1;
		}
	}

	if (sock->challenge) { /* only the server-side has mysqld_version set */
		if (strleq(key, keysize, C("mysqld_version"))) {
			lua_pushinteger(L, sock->challenge->server_version);
			return 1;
		} else if (strleq(key, keysize, C("thread_id"))) {
			lua_pushinteger(L, sock->challenge->thread_id);
			return 1;
		} else if (strleq(key, keysize, C("scramble_buffer"))) {
			lua_pushlstring(L, S(sock->challenge->challenge));
			return 1;
		}
	}
	g_critical("%s: sock->challenge: %p, sock->response: %p (looking for %s)", 
			G_STRLOC,
			sock->challenge,
			sock->response,
			key
			);

	lua_pushnil(L);

	return 1;
}

int network_socket_lua_getmetatable(lua_State *L) {
	static const struct luaL_reg methods[] = {
		{ "__index", proxy_socket_get },
		{ NULL, NULL },
	};
	return proxy_getmetatable(L, methods);
}


