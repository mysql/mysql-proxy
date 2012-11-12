/* $%BEGINLICENSE%$
 Copyright (c) 2008, 2012, Oracle and/or its affiliates. All rights reserved.

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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_FILIO_H
/**
 * required for FIONREAD on solaris
 */
#include <sys/filio.h>
#endif

#ifndef _WIN32
#include <sys/ioctl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define ioctlsocket ioctl
#endif

#include <errno.h>
#include <lua.h>

#include "lua-env.h"
#include "glib-ext.h"

#include "network-mysqld.h"
#include "network-mysqld-packet.h"
#include "chassis-event-thread.h"
#include "network-mysqld-lua.h"

#include "network-conn-pool.h"
#include "network-conn-pool-lua.h"

/**
 * lua wrappers around the connection pool
 */

#define C(x) x, sizeof(x) - 1

/**
 * get the info connection pool 
 *
 * @return nil or requested information
 */
static int proxy_pool_queue_get(lua_State *L) {
	GQueue *queue = *(GQueue **)luaL_checkself(L); 
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	if (strleq(key, keysize, C("cur_idle_connections"))) {
		lua_pushinteger(L, queue ? queue->length : 0);
	} else {
		lua_pushnil(L);
	}

	return 1;
}

int network_connection_pool_queue_getmetatable(lua_State *L) {
	static const struct luaL_reg methods[] = { 
		{ "__index", proxy_pool_queue_get },

		{ NULL, NULL },
	};

	return proxy_getmetatable(L, methods);
}

/**
 * get the info connection pool 
 *
 * @return nil or requested information
 */
static int proxy_pool_users_get(lua_State *L) {
	network_connection_pool *pool = *(network_connection_pool **)luaL_checkself(L); 
	const char *key = luaL_checkstring(L, 2); /** the username */
	GString *s = g_string_new(key);
	GQueue **q_p = NULL;

	q_p = lua_newuserdata(L, sizeof(*q_p)); 
	*q_p = network_connection_pool_get_conns(pool, s, NULL);
	g_string_free(s, TRUE);

	network_connection_pool_queue_getmetatable(L);
	lua_setmetatable(L, -2);

	return 1;
}

int network_connection_pool_users_getmetatable(lua_State *L) {
	static const struct luaL_reg methods[] = {
		{ "__index", proxy_pool_users_get },
		{ NULL, NULL },
	};
	
	return proxy_getmetatable(L, methods);
}

static int proxy_pool_get(lua_State *L) {
	network_connection_pool *pool = *(network_connection_pool **)luaL_checkself(L); 
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	if (strleq(key, keysize, C("max_idle_connections"))) {
		lua_pushinteger(L, pool->max_idle_connections);
	} else if (strleq(key, keysize, C("min_idle_connections"))) {
		lua_pushinteger(L, pool->min_idle_connections);
	} else if (strleq(key, keysize, C("users"))) {
		network_connection_pool **pool_p;

		pool_p = lua_newuserdata(L, sizeof(*pool_p)); 
		*pool_p = pool;

		network_connection_pool_users_getmetatable(L);
		lua_setmetatable(L, -2);
	} else {
		lua_pushnil(L);
	}

	return 1;
}


static int proxy_pool_set(lua_State *L) {
	network_connection_pool *pool = *(network_connection_pool **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	if (strleq(key, keysize, C("max_idle_connections"))) {
		pool->max_idle_connections = lua_tointeger(L, -1);
	} else if (strleq(key, keysize, C("min_idle_connections"))) {
		pool->min_idle_connections = lua_tointeger(L, -1);
	} else {
		return luaL_error(L, "proxy.backend[...].%s is not writable", key);
	}

	return 0;
}

int network_connection_pool_getmetatable(lua_State *L) {
	static const struct luaL_reg methods[] = {
		{ "__index", proxy_pool_get },
		{ "__newindex", proxy_pool_set },
		{ NULL, NULL },
	};

	return proxy_getmetatable(L, methods);
}

/**
 * handle the events of a idling server connection in the pool 
 *
 * make sure we know about connection close from the server side
 * - wait_timeout
 */
static void network_mysqld_con_idle_handle(int event_fd, short events, void *user_data) {
	network_connection_pool_entry *pool_entry = user_data;
	network_connection_pool *pool             = pool_entry->pool;

	if (events == EV_READ) {
		int b = -1;

		/**
		 * @todo we have to handle the case that the server really sent use something
		 *        up to now we just ignore it
		 */
		if (ioctlsocket(event_fd, FIONREAD, &b)) {
			g_critical("ioctl(%d, FIONREAD, ...) failed: %s", event_fd, g_strerror(errno));
		} else if (b != 0) {
			g_critical("ioctl(%d, FIONREAD, ...) said there is something to read, oops: %d", event_fd, b);
		} else {
			/* the server decided the close the connection (wait_timeout, crash, ... )
			 *
			 * remove us from the connection pool and close the connection */
		
			network_connection_pool_remove(pool, pool_entry);
		}
	}
}


/**
 * move the con->server into connection pool and disconnect the 
 * proxy from its backend 
 */
int network_connection_pool_lua_add_connection(network_mysqld_con *con) {
	network_connection_pool_entry *pool_entry = NULL;
	network_mysqld_con_lua_t *st = con->plugin_con_state;

	/* con-server is already disconnected, got out */
	if (!con->server) return 0;

	/* the server connection is still authed */
	con->server->is_authed = 1;

	/* insert the server socket into the connection pool */
	pool_entry = network_connection_pool_add(st->backend->pool, con->server);

	event_set(&(con->server->event), con->server->fd, EV_READ, network_mysqld_con_idle_handle, pool_entry);
	chassis_event_add_local(con->srv, &(con->server->event)); /* add a event, but stay in the same thread */
	
	st->backend->connected_clients--;
	st->backend = NULL;
	st->backend_ndx = -1;
	
	con->server = NULL;

	return 0;
}

/**
 * swap the server connection with a connection from
 * the connection pool
 *
 * we can only switch backends if we have a authed connection in the pool.
 *
 * @return NULL if swapping failed
 *         the new backend on success
 */
network_socket *network_connection_pool_lua_swap(network_mysqld_con *con, int backend_ndx) {
	network_backend_t *backend = NULL;
	network_socket *send_sock;
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	chassis_private *g = con->srv->priv;
	GString empty_username = { "", 0, 0 };

	/*
	 * we can only change to another backend if the backend is already
	 * in the connection pool and connected
	 */

	backend = network_backends_get(g->backends, backend_ndx);
	if (!backend) return NULL;


	/**
	 * get a connection from the pool which matches our basic requirements
	 * - username has to match
	 * - default_db should match
	 */
		
#ifdef DEBUG_CONN_POOL
	g_debug("%s: (swap) check if we have a connection for this user in the pool '%s'", G_STRLOC, con->client->username->str);
#endif
	if (NULL == (send_sock = network_connection_pool_get(backend->pool, 
					con->client->response ? con->client->response->username : &empty_username,
					con->client->default_db))) {
		/**
		 * no connections in the pool
		 */
		st->backend_ndx = -1;
		return NULL;
	}

	/* the backend is up and cool, take and move the current backend into the pool */
#ifdef DEBUG_CONN_POOL
	g_debug("%s: (swap) added the previous connection to the pool", G_STRLOC);
#endif
	network_connection_pool_lua_add_connection(con);

	/* connect to the new backend */
	st->backend = backend;
	st->backend->connected_clients++;
	st->backend_ndx = backend_ndx;

	return send_sock;
}



