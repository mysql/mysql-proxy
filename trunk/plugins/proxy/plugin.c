/* Copyright (C) 2007 MySQL AB

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/** 
 * @page proxy_states The internal states of the Proxy
 *
 * The MySQL Proxy implements the MySQL Protocol in its own way. 
 *
 *   -# connect @msc
 *   client, proxy, backend;
 *   --- [ label = "connect to backend" ];
 *   client->proxy  [ label = "INIT" ];
 *   proxy->backend [ label = "CONNECT_SERVER", URL="\ref proxy_connect_server" ];
 * @endmsc
 *   -# auth @msc
 *   client, proxy, backend;
 *   --- [ label = "authenticate" ];
 *   backend->proxy [ label = "READ_HANDSHAKE", URL="\ref proxy_read_handshake" ];
 *   proxy->client  [ label = "SEND_HANDSHAKE" ];
 *   client->proxy  [ label = "READ_AUTH", URL="\ref proxy_read_auth" ];
 *   proxy->backend [ label = "SEND_AUTH" ];
 *   backend->proxy [ label = "READ_AUTH_RESULT", URL="\ref proxy_read_auth_result" ];
 *   proxy->client  [ label = "SEND_AUTH_RESULT" ];
 * @endmsc
 *   -# query @msc
 *   client, proxy, backend;
 *   --- [ label = "query result phase" ];
 *   client->proxy  [ label = "READ_QUERY", URL="\ref proxy_read_query" ];
 *   proxy->backend [ label = "SEND_QUERY" ];
 *   backend->proxy [ label = "READ_QUERY_RESULT", URL="\ref proxy_read_query_result" ];
 *   proxy->client  [ label = "SEND_QUERY_RESULT", URL="\ref proxy_send_query_result" ];
 * @endmsc
 *
 *   - network_mysqld_proxy_connection_init()
 *     -# registers the callbacks 
 *   - proxy_connect_server() (CON_STATE_CONNECT_SERVER)
 *     -# calls the connect_server() function in the lua script which might decide to
 *       -# send a handshake packet without contacting the backend server (CON_STATE_SEND_HANDSHAKE)
 *       -# closing the connection (CON_STATE_ERROR)
 *       -# picking a active connection from the connection pool
 *       -# pick a backend to authenticate against
 *       -# do nothing 
 *     -# by default, pick a backend from the backend list on the backend with the least active connctions
 *     -# opens the connection to the backend with connect()
 *     -# when done CON_STATE_READ_HANDSHAKE 
 *   - proxy_read_handshake() (CON_STATE_READ_HANDSHAKE)
 *     -# reads the handshake packet from the server 
 *   - proxy_read_auth() (CON_STATE_READ_AUTH)
 *     -# reads the auth packet from the client 
 *   - proxy_read_auth_result() (CON_STATE_READ_AUTH_RESULT)
 *     -# reads the auth-result packet from the server 
 *   - proxy_send_auth_result() (CON_STATE_SEND_AUTH_RESULT)
 *   - proxy_read_query() (CON_STATE_READ_QUERY)
 *     -# reads the query from the client 
 *   - proxy_read_query_result() (CON_STATE_READ_QUERY_RESULT)
 *     -# reads the query-result from the server 
 *   - proxy_send_query_result() (CON_STATE_SEND_QUERY_RESULT)
 *     -# called after the data is written to the client
 *     -# if scripts wants to close connections, goes to CON_STATE_ERROR
 *     -# if queries are in the injection queue, goes to CON_STATE_SEND_QUERY
 *     -# otherwise goes to CON_STATE_READ_QUERY
 *     -# does special handling for COM_BINLOG_DUMP (go to CON_STATE_READ_QUERY_RESULT) 

 */

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

#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>

#include <errno.h>

#include <glib.h>

#ifdef HAVE_LUA_H
/**
 * embedded lua support
 */
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#endif

/* for solaris 2.5 and NetBSD 1.3.x */
#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif


#include <mysqld_error.h> /** for ER_UNKNOWN_ERROR */

#include "network-mysqld.h"
#include "network-mysqld-proto.h"
#include "network-conn-pool.h"
#include "sys-pedantic.h"

#include "sql-tokenizer.h"
#include "lua-load-factory.h"

#define TIME_DIFF_US(t2, t1) \
	        ((t2.tv_sec - t1.tv_sec) * 1000000.0 + (t2.tv_usec - t1.tv_usec))

#define C(x) x, sizeof(x) - 1

#define HASH_INSERT(hash, key, expr) \
		do { \
			GString *hash_value; \
			if ((hash_value = g_hash_table_lookup(hash, key))) { \
				expr; \
			} else { \
				hash_value = g_string_new(NULL); \
				expr; \
				g_hash_table_insert(hash, g_strdup(key), hash_value); \
			} \
		} while(0);

#define CRASHME() do { char *_crashme = NULL; *_crashme = 0; } while(0);

typedef enum {
	PROXY_NO_DECISION,
	PROXY_SEND_QUERY,
	PROXY_SEND_RESULT,
	PROXY_SEND_INJECTION,
	PROXY_IGNORE_RESULT       /** for read_query_result */
} proxy_stmt_ret;

typedef enum { 
	BACKEND_STATE_UNKNOWN, 
	BACKEND_STATE_UP, 
	BACKEND_STATE_DOWN
} backend_state_t;

typedef enum { 
	BACKEND_TYPE_UNKNOWN, 
	BACKEND_TYPE_RW, 
	BACKEND_TYPE_RO
} backend_type_t;

typedef struct {
	network_address addr;

	backend_state_t state;   /**< UP or DOWN */
	backend_type_t type;     /**< ReadWrite or ReadOnly */

	GTimeVal state_since;    /**< timestamp of the last state-change */

	network_connection_pool *pool; /**< the pool of open connections */

	guint connected_clients; /**< number of open connections to this backend for SQF */
} backend_t;

/**
 * the shared information across all connections 
 *
 */
typedef struct {
	/**
	 * our pool if backends
	 *
	 * GPtrArray<backend_t>
	 */
	GPtrArray *backend_pool; 

	GTimeVal backend_last_check;
} plugin_srv_state;

typedef struct {
	/**
	 * the content of the OK packet 
	 */
	int server_status;
	int warning_count;
	guint64 affected_rows;
	guint64 insert_id;

	int was_resultset; /** if set, affected_rows and insert_id are ignored */

	/**
	 * MYSQLD_PACKET_OK or MYSQLD_PACKET_ERR
	 */	
	int query_status;
} query_status;

typedef struct {
	struct {
		GQueue *queries;       /** queries we want to executed */
		query_status qstat;
#ifdef HAVE_LUA_H
		lua_State *L;
		int L_ref;
#endif
		int sent_resultset;    /** make sure we send only one result back to the client */
	} injected;

	plugin_srv_state *global_state;
	backend_t *backend;
	int backend_ndx;

	int connection_close;
} plugin_con_state;

typedef struct {
	GString *query;

	int id; /* a unique id set by the scripts to map the query to a handler */

	/* the userdata's need them */
	GQueue *result_queue; /* the data to parse */
	query_status qstat;

	GTimeVal ts_read_query;          /* timestamp when we added this query to the queues */
	GTimeVal ts_read_query_result_first;   /* when we first finished it */
	GTimeVal ts_read_query_result_last;     /* when we first finished it */
} injection;

struct chassis_plugin_config {
	gchar *address;                   /**< listening address of the proxy */

	gchar **backend_addresses;        /**<    read-write backends */
	gchar **read_only_backend_addresses; /**< read-only  backends */

	gint fix_bug_25371;               /**< suppress the second ERR packet of bug #25371 */

	gint profiling;                   /**< skips the execution of the read_query() function */
	
	gchar *lua_script;                /**< script to load at the start the connection */

	gint start_proxy;

	network_mysqld_con *listen_con;
};

/**
 * compare two strings for equality 
 */
gboolean strleq(const gchar *a, gsize a_len, const gchar *b, gsize b_len) {
	if (a_len != b_len) return FALSE;
	return (0 == strcmp(a, b));
}



static injection *injection_init(int id, GString *query) {
	injection *i;

	i = g_new0(injection, 1);
	i->id = id;
	i->query = query;

	/**
	 * we have to assume that injection_init() is only used by the read_query call
	 * which should be fine
	 */
	g_get_current_time(&(i->ts_read_query));

	return i;
}

static void injection_free(injection *i) {
	if (!i) return;

	if (i->query) g_string_free(i->query, TRUE);

	g_free(i);
}

static plugin_con_state *plugin_con_state_init() {
	plugin_con_state *st;

	st = g_new0(plugin_con_state, 1);

	st->injected.queries = g_queue_new();
	
	return st;
}

static void plugin_con_state_free(plugin_con_state *st) {
	injection *inj;

	if (!st) return;

	while ((inj = g_queue_pop_head(st->injected.queries))) injection_free(inj);
	g_queue_free(st->injected.queries);

	g_free(st);
}

#ifdef HAVE_LUA_H
/**
 * init the global proxy object 
 */
static void proxy_lua_init_global_fenv(lua_State *L) {
	
	lua_newtable(L); /* my empty environment aka {}              (sp += 1) */
#define DEF(x) \
	lua_pushinteger(L, x); \
	lua_setfield(L, -2, #x);
	
	DEF(PROXY_SEND_QUERY);
	DEF(PROXY_SEND_RESULT);
	DEF(PROXY_IGNORE_RESULT);

	DEF(MYSQLD_PACKET_OK);
	DEF(MYSQLD_PACKET_ERR);
	DEF(MYSQLD_PACKET_RAW);

	DEF(BACKEND_STATE_UNKNOWN);
	DEF(BACKEND_STATE_UP);
	DEF(BACKEND_STATE_DOWN);

	DEF(BACKEND_TYPE_UNKNOWN);
	DEF(BACKEND_TYPE_RW);
	DEF(BACKEND_TYPE_RO);

	DEF(COM_SLEEP);
	DEF(COM_QUIT);
	DEF(COM_INIT_DB);
	DEF(COM_QUERY);
	DEF(COM_FIELD_LIST);
	DEF(COM_CREATE_DB);
	DEF(COM_DROP_DB);
	DEF(COM_REFRESH);
	DEF(COM_SHUTDOWN);
	DEF(COM_STATISTICS);
	DEF(COM_PROCESS_INFO);
	DEF(COM_CONNECT);
	DEF(COM_PROCESS_KILL);
	DEF(COM_DEBUG);
	DEF(COM_PING);
	DEF(COM_TIME);
	DEF(COM_DELAYED_INSERT);
	DEF(COM_CHANGE_USER);
	DEF(COM_BINLOG_DUMP);
	DEF(COM_TABLE_DUMP);
	DEF(COM_CONNECT_OUT);
	DEF(COM_REGISTER_SLAVE);
	DEF(COM_STMT_PREPARE);
	DEF(COM_STMT_EXECUTE);
	DEF(COM_STMT_SEND_LONG_DATA);
	DEF(COM_STMT_CLOSE);
	DEF(COM_STMT_RESET);
	DEF(COM_SET_OPTION);
#if MYSQL_VERSION_ID >= 50000
	DEF(COM_STMT_FETCH);
#if MYSQL_VERSION_ID >= 50100
	DEF(COM_DAEMON);
#endif
#endif
	DEF(MYSQL_TYPE_DECIMAL);
#if MYSQL_VERSION_ID >= 50000
	DEF(MYSQL_TYPE_NEWDECIMAL);
#endif
	DEF(MYSQL_TYPE_TINY);
	DEF(MYSQL_TYPE_SHORT);
	DEF(MYSQL_TYPE_LONG);
	DEF(MYSQL_TYPE_FLOAT);
	DEF(MYSQL_TYPE_DOUBLE);
	DEF(MYSQL_TYPE_NULL);
	DEF(MYSQL_TYPE_TIMESTAMP);
	DEF(MYSQL_TYPE_LONGLONG);
	DEF(MYSQL_TYPE_INT24);
	DEF(MYSQL_TYPE_DATE);
	DEF(MYSQL_TYPE_TIME);
	DEF(MYSQL_TYPE_DATETIME);
	DEF(MYSQL_TYPE_YEAR);
	DEF(MYSQL_TYPE_NEWDATE);
	DEF(MYSQL_TYPE_ENUM);
	DEF(MYSQL_TYPE_SET);
	DEF(MYSQL_TYPE_TINY_BLOB);
	DEF(MYSQL_TYPE_MEDIUM_BLOB);
	DEF(MYSQL_TYPE_LONG_BLOB);
	DEF(MYSQL_TYPE_BLOB);
	DEF(MYSQL_TYPE_VAR_STRING);
	DEF(MYSQL_TYPE_STRING);
	DEF(MYSQL_TYPE_GEOMETRY);
#if MYSQL_VERSION_ID >= 50000
	DEF(MYSQL_TYPE_BIT);
#endif

	/* cheat with DEF() a bit :) */
#define PROXY_VERSION PACKAGE_VERSION_ID
	DEF(PROXY_VERSION);
#undef DEF

	/**
	 * create 
	 * - proxy.global 
	 * - proxy.global.config
	 */
	lua_newtable(L);
	lua_newtable(L);
	lua_setfield(L, -2, "config");
	lua_setfield(L, -2, "global");

	lua_setglobal(L, "proxy");
}
#endif

static backend_t *backend_init() {
	backend_t *b;

	b = g_new0(backend_t, 1);

	b->pool = network_connection_pool_init();

	return b;
}

void backend_free(backend_t *b) {
	if (!b) return;

	network_connection_pool_free(b->pool);

	if (b->addr.str) g_free(b->addr.str);

	g_free(b);
}

static plugin_srv_state *plugin_srv_state_init() {
	plugin_srv_state *g;

	g = g_new0(plugin_srv_state, 1);

	g->backend_pool = g_ptr_array_new();

	return g;
}

void plugin_srv_state_free(plugin_srv_state *g) {
	gsize i;

	if (!g) return;

	for (i = 0; i < g->backend_pool->len; i++) {
		backend_t *backend = g->backend_pool->pdata[i];
		
		backend_free(backend);
	}

	g_ptr_array_free(g->backend_pool, TRUE);

	g_free(g);
}

/**
 * parse the result-set packet and extract the fields
 *
 * @param chunk  list of mysql packets 
 * @param fields empty array where the fields shall be stored in
 *
 * @return NULL if there is no resultset
 *         pointer to the chunk after the fields (to the EOF packet)
 */ 
static GList *network_mysqld_result_parse_fields(GList *chunk, GPtrArray *fields) {
	GString *packet = chunk->data;
	guint8 field_count;
	guint i;

	/*
	 * read(6, "\1\0\0\1", 4)                  = 4
	 * read(6, "\2", 1)                        = 1
	 * read(6, "6\0\0\2", 4)                   = 4
	 * read(6, "\3def\0\6STATUS\0\rVariable_name\rVariable_name\f\10\0P\0\0\0\375\1\0\0\0\0", 54) = 54
	 * read(6, "&\0\0\3", 4)                   = 4
	 * read(6, "\3def\0\6STATUS\0\5Value\5Value\f\10\0\0\2\0\0\375\1\0\0\0\0", 38) = 38
	 * read(6, "\5\0\0\4", 4)                  = 4
	 * read(6, "\376\0\0\"\0", 5)              = 5
	 * read(6, "\23\0\0\5", 4)                 = 4
	 * read(6, "\17Aborted_clients\00298", 19) = 19
	 *
	 */

	g_assert(packet->len > NET_HEADER_SIZE);

	/* the first chunk is the length
	 *  */
	if (packet->len != NET_HEADER_SIZE + 1) {
		/*
		 * looks like this isn't a result-set
		 * 
		 *    read(6, "\1\0\0\1", 4)                  = 4
		 *    read(6, "\2", 1)                        = 1
		 *
		 * is expected. We might got called on a non-result, tell the user about it.
		 */
#if 0
		g_debug("%s.%d: network_mysqld_result_parse_fields() got called on a non-resultset. "F_SIZE_T" != 5", __FILE__, __LINE__, packet->len);
#endif

		return NULL;
	}
	
	field_count = packet->str[NET_HEADER_SIZE]; /* the byte after the net-header is the field-count */

	/* the next chunk, the field-def */
	for (i = 0; i < field_count; i++) {
		guint off = NET_HEADER_SIZE;
		MYSQL_FIELD *field;

		chunk = chunk->next;
		packet = chunk->data;

		field = network_mysqld_proto_field_init();

		field->catalog   = network_mysqld_proto_get_lenenc_string(packet, &off);
		field->db        = network_mysqld_proto_get_lenenc_string(packet, &off);
		field->table     = network_mysqld_proto_get_lenenc_string(packet, &off);
		field->org_table = network_mysqld_proto_get_lenenc_string(packet, &off);
		field->name      = network_mysqld_proto_get_lenenc_string(packet, &off);
		field->org_name  = network_mysqld_proto_get_lenenc_string(packet, &off);

		network_mysqld_proto_skip(packet, &off, 1); /* filler */

		field->charsetnr = network_mysqld_proto_get_int16(packet, &off);
		field->length    = network_mysqld_proto_get_int32(packet, &off);
		field->type      = network_mysqld_proto_get_int8(packet, &off);
		field->flags     = network_mysqld_proto_get_int16(packet, &off);
		field->decimals  = network_mysqld_proto_get_int8(packet, &off);

		network_mysqld_proto_skip(packet, &off, 2); /* filler */

		g_ptr_array_add(fields, field);
	}

	/* this should be EOF chunk */
	chunk = chunk->next;
	packet = chunk->data;
	
	g_assert(packet->str[NET_HEADER_SIZE] == MYSQLD_PACKET_EOF);

	return chunk;
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
			g_critical("ioctl(%d, FIONREAD, ...) failed: %s", event_fd, strerror(errno));
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
static int proxy_connection_pool_add_connection(network_mysqld_con *con) {
	chassis *srv = con->srv;
	network_connection_pool_entry *pool_entry = NULL;
	plugin_con_state *st = con->plugin_con_state;

	/* con-server is already disconnected, got out */
	if (!con->server) return 0;

	/* the server connection is still authed */
	con->server->is_authed = 1;

	/* insert the server socket into the connection pool */
	pool_entry = network_connection_pool_add(st->backend->pool, con->server);

	event_set(&(con->server->event), con->server->fd, EV_READ, network_mysqld_con_idle_handle, pool_entry);
	event_base_set(srv->event_base, &(con->server->event));
	event_add(&(con->server->event), NULL);
	
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
static network_socket *proxy_connection_pool_swap(network_mysqld_con *con, int backend_ndx) {
	backend_t *backend = NULL;
	network_socket *send_sock;
	plugin_con_state *st = con->plugin_con_state;
	plugin_srv_state *g = st->global_state;

	/*
	 * we can only change to another backend if the backend is already
	 * in the connection pool and connected
	 */

	if (backend_ndx < 0 || 
	    backend_ndx >= g->backend_pool->len) {
		/* backend_ndx is out of range */
		return NULL;
	} 

	backend = g->backend_pool->pdata[backend_ndx];

	/**
	 * get a connection from the pool which matches our basic requirements
	 * - username has to match
	 * - default_db should match
	 */
		
#ifdef DEBUG_CONN_POOL
	g_debug("%s: (swap) check if we have a connection for this user in the pool '%s'", G_STRLOC, con->client->username->str);
#endif
	if (NULL == (send_sock = network_connection_pool_get(backend->pool, 
					con->client->username,
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
	proxy_connection_pool_add_connection(con);

	/* connect to the new backend */
	st->backend = backend;
	st->backend->connected_clients++;
	st->backend_ndx = backend_ndx;

	return send_sock;
}



#ifdef HAVE_LUA_H
/**
 * taken from lapi.c 
 */
/* convert a stack index to positive */
#define abs_index(L, i)         ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : \
		                                        lua_gettop(L) + (i) + 1)
void lua_getfield_literal (lua_State *L, int idx, const char *k, size_t k_len) {
	idx = abs_index(L, idx);

	lua_pushlstring(L, k, k_len);

	lua_gettable(L, idx);
}

/**
 * check pass through the userdata as is 
 */
static void *luaL_checkself (lua_State *L) {
	return lua_touserdata(L, 1);
}

/**
 * emulate luaL_newmetatable() with lightuserdata instead of strings 
 */
void proxy_getmetatable(lua_State *L, const luaL_reg *methods) {
	/* check if the */

	lua_pushlightuserdata(L, (luaL_reg *)methods);
	lua_gettable(L, LUA_REGISTRYINDEX);

	if (lua_isnil(L, -1)) {
		/* not found */
		lua_pop(L, 1);

		lua_newtable(L);
		luaL_register(L, NULL, methods);

		lua_pushlightuserdata(L, (luaL_reg *)methods);
		lua_pushvalue(L, -2);
		lua_settable(L, LUA_REGISTRYINDEX);
	}
	g_assert(lua_istable(L, -1));
}

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

static const struct luaL_reg methods_proxy_backend_pool_queue[] = { \
	{ "__index", proxy_pool_queue_get },

	{ NULL, NULL },
};

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

	proxy_getmetatable(L, methods_proxy_backend_pool_queue);
	lua_setmetatable(L, -2);

	return 1;
}

static const struct luaL_reg methods_proxy_backend_pool_users[] = {
	{ "__index", proxy_pool_users_get },
	{ NULL, NULL },
};

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

		proxy_getmetatable(L, methods_proxy_backend_pool_users);
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


static const struct luaL_reg methods_proxy_backend_pool[] = {
	{ "__index", proxy_pool_get },
	{ "__newindex", proxy_pool_set },
	{ NULL, NULL },
};

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
	} else if (strleq(key, keysize, C("pool"))) {
		network_connection_pool *pool; 
		network_connection_pool **pool_p;

		pool_p = lua_newuserdata(L, sizeof(pool)); 
		*pool_p = backend->pool;

		proxy_getmetatable(L, methods_proxy_backend_pool);
		lua_setmetatable(L, -2);
	} else {
		lua_pushnil(L);
	}

	return 1;
}

static const struct luaL_reg methods_proxy_backend[] = {
	{ "__index", proxy_backend_get },
	{ NULL, NULL },
};

/**
 * get proxy.backends[ndx]
 *
 * get the backend from the array of mysql backends. 
 *
 * @return nil or the backend
 * @see proxy_backend_get
 */
static int proxy_backends_get(lua_State *L) {
	plugin_con_state *st;
	backend_t *backend; 
	backend_t **backend_p;

	network_mysqld_con *con = *(network_mysqld_con **)luaL_checkself(L);
	int backend_ndx = luaL_checkinteger(L, 2) - 1; /** lua is indexes from 1, C from 0 */

	st = con->plugin_con_state;

	if (backend_ndx < 0 ||
	    backend_ndx >= st->global_state->backend_pool->len) {
		lua_pushnil(L);

		return 1;
	}

	backend = st->global_state->backend_pool->pdata[backend_ndx];

	backend_p = lua_newuserdata(L, sizeof(backend)); /* the table underneat proxy.backends[ndx] */
	*backend_p = backend;

	proxy_getmetatable(L, methods_proxy_backend);
	lua_setmetatable(L, -2);

	return 1;
}

static int proxy_backends_len(lua_State *L) {
	network_mysqld_con *con = *(network_mysqld_con **)luaL_checkself(L);
	plugin_con_state *st;

	st = con->plugin_con_state;

        lua_pushinteger(L, st->global_state->backend_pool->len);

        return 1;
}

static const struct luaL_reg methods_proxy_backends[] = {
	{ "__index", proxy_backends_get },
	{ "__len", proxy_backends_len },
	{ NULL, NULL },
};

static int proxy_socket_get(lua_State *L) {
	network_socket *sock = *(network_socket **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	/**
	 * we to split it in .client and .server here
	 */

	if (strleq(key, keysize, C("default_db"))) {
		lua_pushlstring(L, sock->default_db->str, sock->default_db->len);
	} else if (strleq(key, keysize, C("username"))) {
		lua_pushlstring(L, sock->username->str, sock->username->len);
	} else if (strleq(key, keysize, C("address"))) {
		lua_pushstring(L, sock->addr.str);
	} else if (strleq(key, keysize, C("scrambled_password"))) {
		lua_pushlstring(L, sock->scrambled_password->str, sock->scrambled_password->len);
	} else if (sock->mysqld_version) { /* only the server-side has mysqld_version set */
		if (strleq(key, keysize, C("mysqld_version"))) {
			lua_pushinteger(L, sock->mysqld_version);
		} else if (strleq(key, keysize, C("thread_id"))) {
			lua_pushinteger(L, sock->thread_id);
		} else if (strleq(key, keysize, C("scramble_buffer"))) {
			lua_pushlstring(L, sock->scramble_buf->str, sock->scramble_buf->len);
		} else {
			lua_pushnil(L);
		}
	} else {
		lua_pushnil(L);
	}

	return 1;
}

static const struct luaL_reg methods_proxy_socket[] = {
	{ "__index", proxy_socket_get },
	{ NULL, NULL },
};

/**
 * get the connection information
 *
 * note: might be called in connect_server() before con->server is set 
 */
static int proxy_connection_get(lua_State *L) {
	network_mysqld_con *con = *(network_mysqld_con **)luaL_checkself(L); 
	plugin_con_state *st;
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	st = con->plugin_con_state;

	/**
	 * we to split it in .client and .server here
	 */

	if (strleq(key, keysize, C("default_db"))) {
		return luaL_error(L, "proxy.connection.default_db is deprecated, use proxy.connection.client.default_db or proxy.connection.server.default_db instead");
	} else if (strleq(key, keysize, C("thread_id"))) {
		return luaL_error(L, "proxy.connection.thread_id is deprecated, use proxy.connection.server.thread_id instead");
	} else if (strleq(key, keysize, C("mysqld_version"))) {
		return luaL_error(L, "proxy.connection.mysqld_version is deprecated, use proxy.connection.server.mysqld_version instead");
	} else if (strleq(key, keysize, C("backend_ndx"))) {
		lua_pushinteger(L, st->backend_ndx + 1);
	} else if ((con->server && (strleq(key, keysize, C("server")))) ||
	           (con->client && (strleq(key, keysize, C("client"))))) {
		network_socket **socket_p;

		socket_p = lua_newuserdata(L, sizeof(network_socket)); /* the table underneat proxy.socket */

		if (key[0] == 's') {
			*socket_p = con->server;
		} else {
			*socket_p = con->client;
		}

		proxy_getmetatable(L, methods_proxy_socket);
		lua_setmetatable(L, -2); /* tie the metatable to the table   (sp -= 1) */
	} else {
		lua_pushnil(L);
	}

	return 1;
}

/**
 * set the connection information
 *
 * note: might be called in connect_server() before con->server is set 
 */
static int proxy_connection_set(lua_State *L) {
	network_mysqld_con *con = *(network_mysqld_con **)luaL_checkself(L);
	plugin_con_state *st;
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	st = con->plugin_con_state;

	if (strleq(key, keysize, C("backend_ndx"))) {
		/**
		 * in lua-land the ndx is based on 1, in C-land on 0 */
		int backend_ndx = luaL_checkinteger(L, 3) - 1;
		network_socket *send_sock;
			
		if (backend_ndx == -1) {
			/** drop the backend for now
			 */
			proxy_connection_pool_add_connection(con);
		} else if (NULL != (send_sock = proxy_connection_pool_swap(con, backend_ndx))) {
			con->server = send_sock;
		} else {
			st->backend_ndx = backend_ndx;
		}
	} else if (0 == strcmp(key, "connection_close")) {
		luaL_checktype(L, 3, LUA_TBOOLEAN);

		st->connection_close = lua_toboolean(L, 3);
	} else {
		return luaL_error(L, "proxy.connection.%s is not writable", key);
	}

	return 0;
}

static const struct luaL_reg methods_proxy_connection[] = {
	{ "__index", proxy_connection_get },
	{ "__newindex", proxy_connection_set },
	{ NULL, NULL },
};

static int proxy_queue_append(lua_State *L) {
	/* we expect 2 parameters */
	GQueue *q = *(GQueue **)luaL_checkself(L);
	int resp_type = luaL_checkinteger(L, 2);
	size_t str_len;
	const char *str = luaL_checklstring(L, 3, &str_len);

	GString *query = g_string_sized_new(str_len);
	g_string_append_len(query, str, str_len);

	g_queue_push_tail(q, injection_init(resp_type, query));

	return 0;
}

static int proxy_queue_prepend(lua_State *L) {
	/* we expect 2 parameters */
	GQueue *q = *(GQueue **)luaL_checkself(L);
	int resp_type = luaL_checkinteger(L, 2);
	size_t str_len;
	const char *str = luaL_checklstring(L, 3, &str_len);

	GString *query = g_string_sized_new(str_len);
	g_string_append_len(query, str, str_len);

	g_queue_push_head(q, injection_init(resp_type, query));

	return 0;
}

static int proxy_queue_reset(lua_State *L) {
	/* we expect 2 parameters */
	GQueue *q = *(GQueue **)luaL_checkself(L);
	injection *inj;

	while ((inj = g_queue_pop_head(q))) injection_free(inj);

	return 0;
}

static int proxy_queue_len(lua_State *L) {
	/* we expect 2 parameters */
	GQueue *q = *(GQueue **)luaL_checkself(L);

	lua_pushinteger(L, q->length);

	return 1;
}

static const struct luaL_reg methods_proxy_queue[] = {
	{ "prepend", proxy_queue_prepend },
	{ "append", proxy_queue_append },
	{ "reset", proxy_queue_reset },
	{ "__len", proxy_queue_len },
	{ NULL, NULL },
};

/**
 * split the SQL query into a stream of tokens
 */
static int proxy_tokenize(lua_State *L) {
	size_t str_len;
	const char *str = luaL_checklstring(L, 1, &str_len);
	GPtrArray *tokens = sql_tokens_new();
	gsize i;

	sql_tokenizer(tokens, str, str_len);

	/**
	 * export the data into a table 
	 */

	lua_newtable(L);

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];

		lua_newtable(L);

		lua_pushlstring(L, token->text->str, token->text->len);
		lua_setfield(L, -2, "text");
		
		lua_pushinteger(L, token->token_id);
		lua_setfield(L, -2, "token_id");
		
		lua_pushstring(L, sql_token_get_name(token->token_id));
		lua_setfield(L, -2, "token_name");

		lua_rawseti(L, -2, i + 1);
	}

	sql_tokens_free(tokens);

	return 1;
}
/**
 * setup the script before we hook function is executed 
 *
 * has to be called before any lua_pcall() is called to start a hook function
 *
 * - we use a global lua_State which is split into child-states with lua_newthread()
 * - luaL_ref() moves the state into the registry and cleans up the global stack
 * - on connection close we call luaL_unref() to hand the thread to the GC
 *
 * @see proxy_lua_free_script
 *
 *
 * if the script is cached we have to point the global proxy object to do all l
 *
 */
static int lua_register_callback(network_mysqld_con *con) {
	lua_State *L = NULL;
	plugin_con_state *st   = con->plugin_con_state;

	lua_scope  *sc = con->srv->priv->sc;

	GQueue **q_p;
	network_mysqld_con **con_p;
	chassis_plugin_config *config = con->config;
	int stack_top;

	if (!config->lua_script) return 0;

	if (st->injected.L) {
		/* we have to rewrite _G.proxy to point to the local proxy */
		L = st->injected.L;

		g_assert(lua_isfunction(L, -1));

		lua_getfenv(L, -1);
		g_assert(lua_istable(L, -1));

		lua_getglobal(L, "proxy");
		lua_getmetatable(L, -1); /* meta(_G.proxy) */

		lua_getfield(L, -3, "__proxy"); /* fenv.__proxy */
		lua_setfield(L, -2, "__index"); /* meta[_G.proxy].__index = fenv.__proxy */

		lua_getfield(L, -3, "__proxy"); /* fenv.__proxy */
		lua_setfield(L, -2, "__newindex"); /* meta[_G.proxy].__newindex = fenv.__proxy */

		lua_pop(L, 3);
		
		g_assert(lua_isfunction(L, -1));

		return 0; /* the script-env already setup, get out of here */
	}

	/* a script cache
	 *
	 * we cache the scripts globally in the registry and move a copy of it 
	 * to the new script scope on success.
	 */
	lua_scope_load_script(sc, config->lua_script);

	if (lua_isstring(sc->L, -1)) {
		g_warning("lua_load_file(%s) failed: %s", config->lua_script, lua_tostring(sc->L, -1));

		lua_pop(sc->L, 1); /* remove the error-msg from the stack */
		
		return 0;
	} else if (!lua_isfunction(sc->L, -1)) {
		g_error("luaL_loadfile(%s): returned a %s", config->lua_script, lua_typename(sc->L, lua_type(sc->L, -1)));
	}

	/**
	 * create a side thread for this connection
	 *
	 * (this is not pre-emptive, it is just a new stack in the global env)
	 */
	L = lua_newthread(sc->L);

	st->injected.L_ref = luaL_ref(sc->L, LUA_REGISTRYINDEX);

	stack_top = lua_gettop(L);

	/* get the script from the global stack */
	lua_xmove(sc->L, L, 1);
	g_assert(lua_isfunction(L, -1));

	lua_newtable(L); /* my empty environment aka {}              (sp += 1) 1 */

	lua_newtable(L); /* the meta-table for the new env           (sp += 1) 2 */

	lua_pushvalue(L, LUA_GLOBALSINDEX);                       /* (sp += 1) 3 */
	lua_setfield(L, -2, "__index"); /* { __index = _G }          (sp -= 1) 2 */
	lua_setmetatable(L, -2); /* setmetatable({}, {__index = _G}) (sp -= 1) 1 */

	lua_newtable(L); /* __proxy = { }                            (sp += 1) 2 */

	g_assert(lua_istable(L, -1));

	q_p = lua_newuserdata(L, sizeof(GQueue *));               /* (sp += 1) 3 */
	*q_p = st->injected.queries;

	/**
	 * proxy.queries
	 *
	 * implement a queue
	 *
	 * - append(type, query)
	 * - prepend(type, query)
	 * - reset()
	 * - len() and #proxy.queue
	 *
	 */
	proxy_getmetatable(L, methods_proxy_queue);

	lua_pushvalue(L, -1); /* meta.__index = meta */
	lua_setfield(L, -2, "__index");

	lua_setmetatable(L, -2);


	lua_setfield(L, -2, "queries"); /* proxy.queries = <userdata> */
	
	/**
	 * export internal functions 
	 *
	 * @note: might be moved into a lua-c-lib instead
	 */
	lua_pushcfunction(L, proxy_tokenize);
	lua_setfield(L, -2, "tokenize");

	/**
	 * proxy.connection is (mostly) read-only
	 *
	 * .thread_id  = ... thread-id against this server
	 * .backend_id = ... index into proxy.backends[ndx]
	 *
	 */
	
	con_p = lua_newuserdata(L, sizeof(con));                          /* (sp += 1) */
	*con_p = con;

	proxy_getmetatable(L, methods_proxy_connection);
	lua_setmetatable(L, -2);          /* tie the metatable to the udata   (sp -= 1) */

	lua_setfield(L, -2, "connection"); /* proxy.connection = <udata>     (sp -= 1) */

	/**
	 * register proxy.backends[]
	 *
	 * @see proxy_backends_get()
	 */

	con_p = lua_newuserdata(L, sizeof(con));
	*con_p = con;

	proxy_getmetatable(L, methods_proxy_backends);
	lua_setmetatable(L, -2);          /* tie the metatable to the table   (sp -= 1) */

	lua_setfield(L, -2, "backends");

	/**
	 * proxy.response knows 3 fields with strict types:
	 *
	 * .type = <int>
	 * .errmsg = <string>
	 * .resultset = { 
	 *   fields = { 
	 *     { type = <int>, name = <string > }, 
	 *     { ... } }, 
	 *   rows = { 
	 *     { ..., ... }, 
	 *     { ..., ... } }
	 * }
	 */
	lua_newtable(L);
#if 0
	lua_newtable(L); /* the meta-table for the response-table    (sp += 1) */
	lua_pushcfunction(L, response_get);                       /* (sp += 1) */
	lua_setfield(L, -2, "__index");                           /* (sp -= 1) */
	lua_pushcfunction(L, response_set);                       /* (sp += 1) */
	lua_setfield(L, -2, "__newindex");                        /* (sp -= 1) */
	lua_setmetatable(L, -2); /* tie the metatable to response    (sp -= 1) */
#endif
	lua_setfield(L, -2, "response");
	
	lua_setfield(L, -2, "__proxy");

	/* patch the _G.proxy to point here */
	lua_getglobal(L, "proxy");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);

		proxy_lua_init_global_fenv(L);
	
		lua_getglobal(L, "proxy");
	}
	g_assert(lua_istable(L, -1));

	if (0 == lua_getmetatable(L, -1)) { /* meta(_G.proxy) */
		/* no metatable yet */

		lua_newtable(L);
	}
	g_assert(lua_istable(L, -1));

	lua_getfield(L, -3, "__proxy"); /* fenv.__proxy */
	g_assert(lua_istable(L, -1));
	lua_setfield(L, -2, "__index"); /* meta[_G.proxy].__index = fenv.__proxy */

	lua_getfield(L, -3, "__proxy"); /* fenv.__proxy */
	lua_setfield(L, -2, "__newindex"); /* meta[_G.proxy].__newindex = fenv.__proxy */

	lua_setmetatable(L, -2);

	lua_pop(L, 1);  /* _G.proxy */
	
	g_assert(lua_isfunction(L, -2));
	g_assert(lua_istable(L, -1));

	lua_setfenv(L, -2); /* on the stack should be a modified env (sp -= 1) */

	/* cache the script in this connection */
	g_assert(lua_isfunction(L, -1));
	lua_pushvalue(L, -1);

	/* run the script once to get the functions set in the global scope */
	if (lua_pcall(L, 0, 0, 0) != 0) {
		g_critical("(lua-error) [%s]\n%s", config->lua_script, lua_tostring(L, -1));

		lua_pop(L, 1); /* errmsg */
	
		luaL_unref(sc->L, LUA_REGISTRYINDEX, sc->L_ref);

		return 0;
	}

	st->injected.L = L;
	
	g_assert(lua_isfunction(L, -1));
	g_assert(lua_gettop(L) - stack_top == 1);

	return 0;
}

/**
 * handle the proxy.response.* table from the lua script
 *
 * proxy.response
 *   .type can be either ERR, OK or RAW
 *   .resultset (in case of OK)
 *     .fields
 *     .rows
 *   .errmsg (in case of ERR)
 *   .packet (in case of nil)
 *
 */
static int proxy_lua_handle_proxy_response(network_mysqld_con *con) {
	plugin_con_state *st = con->plugin_con_state;
	int resp_type = 1;
	const char *str;
	size_t str_len;
	lua_State *L = st->injected.L;
	chassis_plugin_config *config = con->config;

	/**
	 * on the stack should be the fenv of our function */
	g_assert(lua_istable(L, -1));
	
	lua_getfield(L, -1, "proxy"); /* proxy.* from the env  */
	g_assert(lua_istable(L, -1));

	lua_getfield(L, -1, "response"); /* proxy.response */
	if (lua_isnil(L, -1)) {
		g_message("%s.%d: proxy.response isn't set in %s", __FILE__, __LINE__, 
				config->lua_script);

		lua_pop(L, 2); /* proxy + nil */

		return -1;
	} else if (!lua_istable(L, -1)) {
		g_message("%s.%d: proxy.response has to be a table, is %s in %s", __FILE__, __LINE__,
				lua_typename(L, lua_type(L, -1)),
				config->lua_script);

		lua_pop(L, 2); /* proxy + response */
		return -1;
	}

	lua_getfield(L, -1, "type"); /* proxy.response.type */
	if (lua_isnil(L, -1)) {
		/**
		 * nil is fine, we expect to get a raw packet in that case
		 */
		g_message("%s.%d: proxy.response.type isn't set in %s", __FILE__, __LINE__, 
				config->lua_script);

		lua_pop(L, 3); /* proxy + nil */

		return -1;

	} else if (!lua_isnumber(L, -1)) {
		g_message("%s.%d: proxy.response.type has to be a number, is %s in %s", __FILE__, __LINE__,
				lua_typename(L, lua_type(L, -1)),
				config->lua_script);
		
		lua_pop(L, 3); /* proxy + response + type */

		return -1;
	} else {
		resp_type = lua_tonumber(L, -1);
	}
	lua_pop(L, 1);

	switch(resp_type) {
	case MYSQLD_PACKET_OK: {
		GPtrArray *fields = NULL;
		GPtrArray *rows = NULL;
		gsize field_count = 0;

		lua_getfield(L, -1, "resultset"); /* proxy.response.resultset */
		if (lua_istable(L, -1)) {
			guint i;
			lua_getfield(L, -1, "fields"); /* proxy.response.resultset.fields */
			g_assert(lua_istable(L, -1));

			fields = g_ptr_array_new();
		
			for (i = 1, field_count = 0; ; i++, field_count++) {
				lua_rawgeti(L, -1, i);
				
				if (lua_istable(L, -1)) { /** proxy.response.resultset.fields[i] */
					MYSQL_FIELD *field;
	
					field = network_mysqld_proto_field_init();
	
					lua_getfield(L, -1, "name"); /* proxy.response.resultset.fields[].name */
	
					if (!lua_isstring(L, -1)) {
						field->name = g_strdup("no-field-name");
	
						g_warning("%s.%d: proxy.response.type = OK, "
								"but proxy.response.resultset.fields[%u].name is not a string (is %s), "
								"using default", 
								__FILE__, __LINE__,
								i,
								lua_typename(L, lua_type(L, -1)));
					} else {
						field->name = g_strdup(lua_tostring(L, -1));
					}
					lua_pop(L, 1);
	
					lua_getfield(L, -1, "type"); /* proxy.response.resultset.fields[].type */
					if (!lua_isnumber(L, -1)) {
						g_warning("%s.%d: proxy.response.type = OK, "
								"but proxy.response.resultset.fields[%u].type is not a integer (is %s), "
								"using MYSQL_TYPE_STRING", 
								__FILE__, __LINE__,
								i,
								lua_typename(L, lua_type(L, -1)));
	
						field->type = MYSQL_TYPE_STRING;
					} else {
						field->type = lua_tonumber(L, -1);
					}
					lua_pop(L, 1);
					field->flags = PRI_KEY_FLAG;
					field->length = 32;
					g_ptr_array_add(fields, field);
					
					lua_pop(L, 1); /* pop key + value */
				} else if (lua_isnil(L, -1)) {
					lua_pop(L, 1); /* pop the nil and leave the loop */
					break;
				} else {
					g_error("proxy.response.resultset.fields[%d] should be a table, but is a %s", 
							i,
							lua_typename(L, lua_type(L, -1)));
				}
			}
			lua_pop(L, 1);
	
			rows = g_ptr_array_new();
			lua_getfield(L, -1, "rows"); /* proxy.response.resultset.rows */
			g_assert(lua_istable(L, -1));
			for (i = 1; ; i++) {
				lua_rawgeti(L, -1, i);
	
				if (lua_istable(L, -1)) { /** proxy.response.resultset.rows[i] */
					GPtrArray *row;
					gsize j;
	
					row = g_ptr_array_new();
	
					/* we should have as many columns as we had fields */
		
					for (j = 1; j < field_count + 1; j++) {
						lua_rawgeti(L, -1, j);
	
						if (lua_isnil(L, -1)) {
							g_ptr_array_add(row, NULL);
						} else {
							g_ptr_array_add(row, g_strdup(lua_tostring(L, -1)));
						}
	
						lua_pop(L, 1);
					}
	
					g_ptr_array_add(rows, row);
	
					lua_pop(L, 1); /* pop value */
				} else if (lua_isnil(L, -1)) {
					lua_pop(L, 1); /* pop the nil and leave the loop */
					break;
				} else {
					g_error("proxy.response.resultset.rows[%d] should be a table, but is a %s", 
							i,
							lua_typename(L, lua_type(L, -1)));
				}
			}
			lua_pop(L, 1);

			network_mysqld_con_send_resultset(con->client, fields, rows);
		} else {
			guint64 affected_rows = 0;
			guint64 insert_id = 0;

			lua_getfield(L, -2, "affected_rows"); /* proxy.response.affected_rows */
			if (lua_isnumber(L, -1)) {
				affected_rows = lua_tonumber(L, -1);
			}
			lua_pop(L, 1);

			lua_getfield(L, -2, "insert_id"); /* proxy.response.affected_rows */
			if (lua_isnumber(L, -1)) {
				insert_id = lua_tonumber(L, -1);
			}
			lua_pop(L, 1);

			network_mysqld_con_send_ok_full(con->client, affected_rows, insert_id, 0x0002, 0);
		}

		/**
		 * someone should cleanup 
		 */
		if (fields) {
			network_mysqld_proto_fields_free(fields);
			fields = NULL;
		}

		if (rows) {
			guint i;
			for (i = 0; i < rows->len; i++) {
				GPtrArray *row = rows->pdata[i];
				guint j;

				for (j = 0; j < row->len; j++) {
					if (row->pdata[j]) g_free(row->pdata[j]);
				}

				g_ptr_array_free(row, TRUE);
			}
			g_ptr_array_free(rows, TRUE);
			rows = NULL;
		}

		
		lua_pop(L, 1); /* .resultset */
		
		break; }
	case MYSQLD_PACKET_ERR: {
		gint errorcode = ER_UNKNOWN_ERROR;
		const gchar *sqlstate = "07000"; /** let's call ourself Dynamic SQL ... 07000 is "dynamic SQL error" */
		
		lua_getfield(L, -1, "errcode"); /* proxy.response.errcode */
		if (lua_isnumber(L, -1)) {
			errorcode = lua_tonumber(L, -1);
		}
		lua_pop(L, 1);

		lua_getfield(L, -1, "sqlstate"); /* proxy.response.sqlstate */
		sqlstate = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, -1, "errmsg"); /* proxy.response.errmsg */
		if (lua_isstring(L, -1)) {
			str = lua_tolstring(L, -1, &str_len);

			network_mysqld_con_send_error_full(con->client, str, str_len, errorcode, sqlstate);
		} else {
			network_mysqld_con_send_error(con->client, C("(lua) proxy.response.errmsg is nil"));
		}
		lua_pop(L, 1);

		break; }
	case MYSQLD_PACKET_RAW: {
		guint i;
		/**
		 * iterate over the packet table and add each packet to the send-queue
		 */
		lua_getfield(L, -1, "packets"); /* proxy.response.packets */
		if (lua_isnil(L, -1)) {
			g_message("%s.%d: proxy.response.packets isn't set in %s", __FILE__, __LINE__,
					config->lua_script);

			lua_pop(L, 3 + 1); /* fenv + proxy + response + nil */

			return -1;
		} else if (!lua_istable(L, -1)) {
			g_message("%s.%d: proxy.response.packets has to be a table, is %s in %s", __FILE__, __LINE__,
					lua_typename(L, lua_type(L, -1)),
					config->lua_script);

			lua_pop(L, 3 + 1); /* fenv + proxy + response + packets */
			return -1;
		}

		for (i = 1; ; i++) {
			lua_rawgeti(L, -1, i);

			if (lua_isstring(L, -1)) { /** proxy.response.packets[i] */
				str = lua_tolstring(L, -1, &str_len);

				network_queue_append(con->client->send_queue, str, str_len, con->client->packet_id++);
	
				lua_pop(L, 1); /* pop value */
			} else if (lua_isnil(L, -1)) {
				lua_pop(L, 1); /* pop the nil and leave the loop */
				break;
			} else {
				g_error("%s.%d: proxy.response.packets should be array of strings, field %u was %s", 
						__FILE__, __LINE__, 
						i,
						lua_typename(L, lua_type(L, -1)));
			}
		}

		lua_pop(L, 1); /* .packets */

		break; }
	default:
		g_message("proxy.response.type is unknown: %d", resp_type);

		lua_pop(L, 2); /* proxy + response */

		return -1;
	}

	lua_pop(L, 2);

	return 0;
}
#endif

#ifdef HAVE_LUA_H
/**
 * parsed result set
 *
 * 
 */
typedef struct {
	GQueue *result_queue;   /**< where the packets are read from */

	GPtrArray *fields;      /**< the parsed fields */

	GList *rows_chunk_head; /**< pointer to the EOF packet after the fields */
	GList *row;             /**< the current row */

	query_status qstat;     /**< state if this query */
} proxy_resultset_t;

proxy_resultset_t *proxy_resultset_init() {
	proxy_resultset_t *res;

	res = g_new0(proxy_resultset_t, 1);

	return res;
}

void proxy_resultset_free(proxy_resultset_t *res) {
	if (!res) return;

	if (res->fields) {
		network_mysqld_proto_fields_free(res->fields);
	}

	g_free(res);
}

static int proxy_resultset_gc(lua_State *L) {
	proxy_resultset_t *res = *(proxy_resultset_t **)lua_touserdata(L, 1);
	
	proxy_resultset_free(res);

	return 0;
}

static int proxy_resultset_gc_light(lua_State *L) {
	proxy_resultset_t *res = *(proxy_resultset_t **)lua_touserdata(L, 1);
	
	g_free(res);

	return 0;
}

static int proxy_resultset_fields_len(lua_State *L) {
	GPtrArray *fields = *(GPtrArray **)luaL_checkself(L);
        lua_pushinteger(L, fields->len);
        return 1;
}

static int proxy_resultset_field_get(lua_State *L) {
	MYSQL_FIELD *field = *(MYSQL_FIELD **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);


	if (strleq(key, keysize, C("type"))) {
		lua_pushinteger(L, field->type);
	} else if (strleq(key, keysize, C("name"))) {
		lua_pushstring(L, field->name);
	} else if (strleq(key, keysize, C("org_name"))) {
		lua_pushstring(L, field->org_name);
	} else if (strleq(key, keysize, C("org_table"))) {
		lua_pushstring(L, field->org_table);
	} else if (strleq(key, keysize, C("table"))) {
		lua_pushstring(L, field->table);
	} else {
		lua_pushnil(L);
	}

	return 1;
}

static const struct luaL_reg methods_proxy_resultset_fields_field[] = {
	{ "__index", proxy_resultset_field_get },
	{ NULL, NULL },
};

/**
 * get a field from the result-set
 *
 */
static int proxy_resultset_fields_get(lua_State *L) {
	GPtrArray *fields = *(GPtrArray **)luaL_checkself(L);
	MYSQL_FIELD *field;
	MYSQL_FIELD **field_p;
	int ndx = luaL_checkinteger(L, 2);

	if (ndx < 1 || ndx > fields->len) {
		lua_pushnil(L);

		return 1;
	}

	field = fields->pdata[ndx - 1]; /** lua starts at 1, C at 0 */

	field_p = lua_newuserdata(L, sizeof(field));
	*field_p = field;

	proxy_getmetatable(L, methods_proxy_resultset_fields_field);
	lua_setmetatable(L, -2);

	return 1;
}

/**
 * get the next row from the resultset
 *
 * returns a lua-table with the fields (starting at 1)
 *
 * @return 0 on error, 1 on success
 *
 */
static int proxy_resultset_rows_iter(lua_State *L) {
	proxy_resultset_t *res = *(proxy_resultset_t **)lua_touserdata(L, lua_upvalueindex(1));
	guint32 off = NET_HEADER_SIZE; /* skip the packet-len and sequence-number */
	GString *packet;
	GPtrArray *fields = res->fields;
	gsize i;

	GList *chunk = res->row;

	if (chunk == NULL) return 0;

	packet = chunk->data;

	/* if we find the 2nd EOF packet we are done */
	if (packet->str[off] == MYSQLD_PACKET_EOF &&
	    packet->len < 10) return 0;

	/* a ERR packet instead of real rows
	 *
	 * like "explain select fld3 from t2 ignore index (fld3,not_existing)"
	 *
	 * see mysql-test/t/select.test
	 *  */
	if (packet->str[off] == MYSQLD_PACKET_ERR) {
		return 0;
	}

	lua_newtable(L);

	for (i = 0; i < fields->len; i++) {
		guint64 field_len;

		g_assert(off <= packet->len + NET_HEADER_SIZE);

		field_len = network_mysqld_proto_get_lenenc_int(packet, &off);

		if (field_len == 251) { /** @todo use constant */
			lua_pushnil(L);
			
			off += 0;
		} else {
			/**
			 * @todo we only support fields in the row-iterator < 16M (packet-len)
			 */
			g_assert(field_len <= packet->len + NET_HEADER_SIZE);
			g_assert(off + field_len <= packet->len + NET_HEADER_SIZE);

			lua_pushlstring(L, packet->str + off, field_len);

			off += field_len;
		}

		/* lua starts its tables at 1 */
		lua_rawseti(L, -2, i + 1);
	}

	res->row = res->row->next;

	return 1;
}

/**
 * parse the result-set of the query
 *
 * @return if this is not a result-set we return -1
 */
static int parse_resultset_fields(proxy_resultset_t *res) {
	GString *packet = res->result_queue->head->data;
	GList *chunk;

	if (res->fields) return 0;

	switch (packet->str[NET_HEADER_SIZE]) {
	case MYSQLD_PACKET_OK:
	case MYSQLD_PACKET_ERR:
		res->qstat.query_status = packet->str[NET_HEADER_SIZE];

		return 0;
	default:
		/* OK with a resultset */
		res->qstat.query_status = MYSQLD_PACKET_OK;
		break;
	}

	/* parse the fields */
	res->fields = network_mysqld_proto_fields_init();

	if (!res->fields) return -1;

	chunk = network_mysqld_result_parse_fields(res->result_queue->head, res->fields);

	/* no result-set found */
	if (!chunk) return -1;

	/* skip the end-of-fields chunk */
	res->rows_chunk_head = chunk->next;

	return 0;
}

static const struct luaL_reg methods_proxy_resultset_fields[] = {
	{ "__index", proxy_resultset_fields_get },
	{ "__len", proxy_resultset_fields_len },
	{ NULL, NULL },
};

static const struct luaL_reg methods_proxy_resultset_light[] = {
	{ "__gc", proxy_resultset_gc_light },
	{ NULL, NULL },
};

static int proxy_resultset_get(lua_State *L) {
	proxy_resultset_t *res = *(proxy_resultset_t **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	if (strleq(key, keysize, C("fields"))) {
		GPtrArray **fields_p;

		parse_resultset_fields(res);

		if (res->fields) {
			fields_p = lua_newuserdata(L, sizeof(res->fields));
			*fields_p = res->fields;

			proxy_getmetatable(L, methods_proxy_resultset_fields);
			lua_setmetatable(L, -2); /* tie the metatable to the table   (sp -= 1) */
		} else {
			lua_pushnil(L);
		}
	} else if (strleq(key, keysize, C("rows"))) {
		proxy_resultset_t *rows;
		proxy_resultset_t **rows_p;

		parse_resultset_fields(res);

		if (res->rows_chunk_head) {
	
			rows = proxy_resultset_init();
			rows->rows_chunk_head = res->rows_chunk_head;
			rows->row    = rows->rows_chunk_head;
			rows->fields = res->fields;
	
			/* push the parameters on the stack */
			rows_p = lua_newuserdata(L, sizeof(rows));
			*rows_p = rows;

			proxy_getmetatable(L, methods_proxy_resultset_light);
			lua_setmetatable(L, -2);
	
			/* return a interator */
			lua_pushcclosure(L, proxy_resultset_rows_iter, 1);
		} else {
			lua_pushnil(L);
		}
	} else if (strleq(key, keysize, C("raw"))) {
		GString *s = res->result_queue->head->data;
		lua_pushlstring(L, s->str + 4, s->len - 4);
	} else if (strleq(key, keysize, C("flags"))) {
		lua_newtable(L);
		lua_pushboolean(L, (res->qstat.server_status & SERVER_STATUS_IN_TRANS) != 0);
		lua_setfield(L, -2, "in_trans");

		lua_pushboolean(L, (res->qstat.server_status & SERVER_STATUS_AUTOCOMMIT) != 0);
		lua_setfield(L, -2, "auto_commit");
		
		lua_pushboolean(L, (res->qstat.server_status & SERVER_QUERY_NO_GOOD_INDEX_USED) != 0);
		lua_setfield(L, -2, "no_good_index_used");
		
		lua_pushboolean(L, (res->qstat.server_status & SERVER_QUERY_NO_INDEX_USED) != 0);
		lua_setfield(L, -2, "no_index_used");
	} else if (strleq(key, keysize, C("warning_count"))) {
		lua_pushinteger(L, res->qstat.warning_count);
	} else if (strleq(key, keysize, C("affected_rows"))) {
		/**
		 * if the query had a result-set (SELECT, ...) 
		 * affected_rows and insert_id are not valid
		 */
		if (res->qstat.was_resultset) {
			lua_pushnil(L);
		} else {
			lua_pushnumber(L, res->qstat.affected_rows);
		}
	} else if (strleq(key, keysize, C("insert_id"))) {
		if (res->qstat.was_resultset) {
			lua_pushnil(L);
		} else {
			lua_pushnumber(L, res->qstat.insert_id);
		}
	} else if (strleq(key, keysize, C("query_status"))) {
		if (0 != parse_resultset_fields(res)) {
			/* not a result-set */
			lua_pushnil(L);
		} else {
			lua_pushinteger(L, res->qstat.query_status);
		}
	} else {
		lua_pushnil(L);
	}

	return 1;
}

static const struct luaL_reg methods_proxy_resultset[] = {
	{ "__index", proxy_resultset_get },
	{ "__gc", proxy_resultset_gc },
	{ NULL, NULL },
};

static int proxy_injection_get(lua_State *L) {
	injection *inj = *(injection **)luaL_checkself(L);
	gsize keysize = 0;
	const char *key = luaL_checklstring(L, 2, &keysize);

	if (strleq(key, keysize, C("type"))) {
		lua_pushinteger(L, inj->id); /** DEPRECATED: use "inj.id" instead */
	} else if (strleq(key, keysize, C("id"))) {
		lua_pushinteger(L, inj->id);
	} else if (strleq(key, keysize, C("query"))) {
		lua_pushlstring(L, inj->query->str, inj->query->len);
	} else if (strleq(key, keysize, C("query_time"))) {
		lua_pushinteger(L, TIME_DIFF_US(inj->ts_read_query_result_first, inj->ts_read_query));
	} else if (strleq(key, keysize, C("response_time"))) {
		lua_pushinteger(L, TIME_DIFF_US(inj->ts_read_query_result_last, inj->ts_read_query));
	} else if (strleq(key, keysize, C("resultset"))) {
		/* fields, rows */
		proxy_resultset_t *res;
		proxy_resultset_t **res_p;

		res_p = lua_newuserdata(L, sizeof(res));
		*res_p = res = proxy_resultset_init();

		res->result_queue = inj->result_queue;
		res->qstat = inj->qstat;

		proxy_getmetatable(L, methods_proxy_resultset);
		lua_setmetatable(L, -2);
	} else {
		g_message("%s.%d: inj[%s] ... not found", __FILE__, __LINE__, key);

		lua_pushnil(L);
	}

	return 1;
}

static const struct luaL_reg methods_proxy_injection[] = {
	{ "__index", proxy_injection_get },
	{ NULL, NULL },
};


#endif
static proxy_stmt_ret proxy_lua_read_query_result(network_mysqld_con *con) {
#ifdef HAVE_LUA_H
	network_socket *send_sock = con->client;
#endif
	injection *inj = NULL;
	plugin_con_state *st = con->plugin_con_state;
	proxy_stmt_ret ret = PROXY_NO_DECISION;

	/**
	 * check if we want to forward the statement to the client 
	 *
	 * if not, clean the send-queue 
	 */

	if (0 == st->injected.queries->length) return PROXY_NO_DECISION;

	inj = g_queue_pop_head(st->injected.queries);

#ifdef HAVE_LUA_H
	/* call the lua script to pick a backend
	 * */
	lua_register_callback(con);

	if (st->injected.L) {
		lua_State *L = st->injected.L;

		g_assert(lua_isfunction(L, -1));
		lua_getfenv(L, -1);
		g_assert(lua_istable(L, -1));
		
		lua_getfield_literal(L, -1, C("read_query_result"));
		if (lua_isfunction(L, -1)) {
			injection **inj_p;
			GString *packet;

			inj_p = lua_newuserdata(L, sizeof(inj));
			*inj_p = inj;

			inj->result_queue = con->client->send_queue->chunks;
			inj->qstat = st->injected.qstat;

			proxy_getmetatable(L, methods_proxy_injection);
			lua_setmetatable(L, -2);

			if (lua_pcall(L, 1, 1, 0) != 0) {
				g_critical("(read_query_result) %s", lua_tostring(L, -1));

				lua_pop(L, 1); /* err-msg */

				ret = PROXY_NO_DECISION;
			} else {
				if (lua_isnumber(L, -1)) {
					ret = lua_tonumber(L, -1);
				}
				lua_pop(L, 1);
			}

			switch (ret) {
			case PROXY_SEND_RESULT:
				/**
				 * replace the result-set the server sent us 
				 */
				while ((packet = g_queue_pop_head(send_sock->send_queue->chunks))) g_string_free(packet, TRUE);
				
				/**
				 * we are a response to the client packet, hence one packet id more 
				 */
				send_sock->packet_id++;

				if (proxy_lua_handle_proxy_response(con)) {
					/**
					 * handling proxy.response failed
					 *
					 * send a ERR packet in case there was no result-set sent yet
					 */
			
					if (!st->injected.sent_resultset) {
						network_mysqld_con_send_error(con->client, C("(lua) handling proxy.response failed, check error-log"));
					}
				}

				/* fall through */
			case PROXY_NO_DECISION:
				if (!st->injected.sent_resultset) {
					/**
					 * make sure we send only one result-set per client-query
					 */
					st->injected.sent_resultset++;
					break;
				}
				g_warning("%s.%d: got asked to send a resultset, but ignoring it as we already have sent %d resultset(s). injection-id: %d",
						__FILE__, __LINE__,
						st->injected.sent_resultset,
						inj->id);

				st->injected.sent_resultset++;

				/* fall through */
			case PROXY_IGNORE_RESULT:
				/* trash the packets for the injection query */
				while ((packet = g_queue_pop_head(send_sock->send_queue->chunks))) g_string_free(packet, TRUE);

				break;
			default:
				/* invalid return code */
				g_message("%s.%d: return-code for read_query_result() was neither PROXY_SEND_RESULT or PROXY_IGNORE_RESULT, will ignore the result",
						__FILE__, __LINE__);

				while ((packet = g_queue_pop_head(send_sock->send_queue->chunks))) g_string_free(packet, TRUE);

				break;
			}

		} else if (lua_isnil(L, -1)) {
			/* no function defined, let's send the result-set */
			lua_pop(L, 1); /* pop the nil */
		} else {
			g_message("%s.%d: (network_mysqld_con_handle_proxy_resultset) got wrong type: %s", __FILE__, __LINE__, lua_typename(L, lua_type(L, -1)));
			lua_pop(L, 1); /* pop the nil */
		}
		lua_pop(L, 1); /* fenv */

		g_assert(lua_isfunction(L, -1));
	}
#endif

	injection_free(inj);

	return ret;
}

/**
 * call the lua function to intercept the handshake packet
 *
 * @return PROXY_SEND_QUERY  to send the packet from the client
 *         PROXY_NO_DECISION to pass the server packet unmodified
 */
static proxy_stmt_ret proxy_lua_read_handshake(network_mysqld_con *con) {
	proxy_stmt_ret ret = PROXY_NO_DECISION; /* send what the server gave us */
#ifdef HAVE_LUA_H
	plugin_con_state *st = con->plugin_con_state;
	network_socket   *recv_sock = con->server;
	network_socket   *send_sock = con->client;

	lua_State *L;

	/* call the lua script to pick a backend
	 * */
	lua_register_callback(con);

	if (!st->injected.L) return ret;

	L = st->injected.L;

	g_assert(lua_isfunction(L, -1));
	lua_getfenv(L, -1);
	g_assert(lua_istable(L, -1));
	
	lua_getfield_literal(L, -1, C("read_handshake"));
	if (lua_isfunction(L, -1)) {
		/* export
		 *
		 * every thing we know about it
		 *  */

		lua_newtable(L);

		lua_pushlstring(L, recv_sock->scramble_buf->str, recv_sock->scramble_buf->len);
		lua_setfield(L, -2, "scramble");
		lua_pushinteger(L, recv_sock->mysqld_version);
		lua_setfield(L, -2, "mysqld_version");
		lua_pushinteger(L, recv_sock->thread_id);
		lua_setfield(L, -2, "thread_id");
		lua_pushstring(L, recv_sock->addr.str);
		lua_setfield(L, -2, "server_addr");
		lua_pushstring(L, send_sock->addr.str);
		lua_setfield(L, -2, "client_addr");

		if (lua_pcall(L, 1, 1, 0) != 0) {
			g_critical("(read_handshake) %s", lua_tostring(L, -1));

			lua_pop(L, 1); /* errmsg */

			/* the script failed, but we have a useful default */
		} else {
			if (lua_isnumber(L, -1)) {
				ret = lua_tonumber(L, -1);
			}
			lua_pop(L, 1);
		}
	
		switch (ret) {
		case PROXY_NO_DECISION:
			break;
		case PROXY_SEND_QUERY:
			g_warning("%s.%d: (read_handshake) return proxy.PROXY_SEND_QUERY is deprecated, use PROXY_SEND_RESULT instead",
					__FILE__, __LINE__);

			ret = PROXY_SEND_RESULT;
		case PROXY_SEND_RESULT:
			/**
			 * proxy.response.type = ERR, RAW, ...
			 */

			if (proxy_lua_handle_proxy_response(con)) {
				/**
				 * handling proxy.response failed
				 *
				 * send a ERR packet
				 */
		
				network_mysqld_con_send_error(con->client, C("(lua) handling proxy.response failed, check error-log"));
			}

			break;
		default:
			ret = PROXY_NO_DECISION;
			break;
		}
	} else if (lua_isnil(L, -1)) {
		lua_pop(L, 1); /* pop the nil */
	} else {
		g_message("%s.%d: %s", __FILE__, __LINE__, lua_typename(L, lua_type(L, -1)));
		lua_pop(L, 1); /* pop the ... */
	}
	lua_pop(L, 1); /* fenv */

	g_assert(lua_isfunction(L, -1));
#endif
	return ret;
}


/**
 * parse the hand-shake packet from the server
 *
 *
 * @note the SSL and COMPRESS flags are disabled as we can't 
 *       intercept or parse them.
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_read_handshake) {
	GString *packet;
	GList *chunk;
	network_socket *recv_sock, *send_sock;
	guint off = 0;
	int maj, min, patch;
	guint16 server_cap = 0;
	guint8  server_lang = 0;
	guint16 server_status = 0;
	gchar *scramble_1, *scramble_2;

	send_sock = con->client;
	recv_sock = con->server;

	chunk = recv_sock->recv_queue->chunks->tail;
	packet = chunk->data;

	if (packet->len != recv_sock->packet_len + NET_HEADER_SIZE) {
		/**
		 * packet is too short, looks nasty.
		 *
		 * report an error and let the core send a error to the 
		 * client
		 */

		recv_sock->packet_len = PACKET_LEN_UNSET;
		g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

		return RET_ERROR;
	}

	if (packet->str[NET_HEADER_SIZE + 0] == '\xff') {
		/* the server doesn't like us and sends a ERR packet
		 *
		 * forward it to the client */

		network_queue_append_chunk(send_sock->send_queue, packet);

		recv_sock->packet_len = PACKET_LEN_UNSET;
		g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

		return RET_ERROR;
	} else if (packet->str[NET_HEADER_SIZE + 0] != '\x0a') {
		/* the server isn't 4.1+ server, send a client a ERR packet
		 */
		recv_sock->packet_len = PACKET_LEN_UNSET;
		g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

		network_mysqld_con_send_error(send_sock, C("unknown protocol"));

		return RET_ERROR;
	}

	/* scan for a \0 */
	for (off = NET_HEADER_SIZE + 1; packet->str[off] && off < packet->len + NET_HEADER_SIZE; off++);

	if (packet->str[off] != '\0') {
		/* the server has sent us garbage */
		recv_sock->packet_len = PACKET_LEN_UNSET;
		g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

		network_mysqld_con_send_error(send_sock, C("protocol 10, but version number not terminated"));

		return RET_ERROR;
	}

	if (3 != sscanf(packet->str + NET_HEADER_SIZE + 1, "%d.%d.%d%*s", &maj, &min, &patch)) {
		/* can't parse the protocol */
		recv_sock->packet_len = PACKET_LEN_UNSET;
		g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

		network_mysqld_con_send_error(send_sock, C("protocol 10, but version number not parsable"));

		return RET_ERROR;
	}

	/**
	 * out of range 
	 */
	if (min   < 0 || min   > 100 ||
	    patch < 0 || patch > 100 ||
	    maj   < 0 || maj   > 10) {
		recv_sock->packet_len = PACKET_LEN_UNSET;
		g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

		network_mysqld_con_send_error(send_sock, C("protocol 10, but version number out of range"));

		return RET_ERROR;
	}

	recv_sock->mysqld_version = 
		maj * 10000 +
		min *   100 +
		patch;

	/* skip the \0 */
	off++;

	recv_sock->thread_id = network_mysqld_proto_get_int32(packet, &off);
	send_sock->thread_id = recv_sock->thread_id;

	/**
	 * get the scramble buf
	 *
	 * 8 byte here and some the other 12 somewhen later
	 */	
	scramble_1 = network_mysqld_proto_get_string_len(packet, &off, 8);

	network_mysqld_proto_skip(packet, &off, 1);

	/* we can't sniff compressed packets nor do we support SSL */
	packet->str[off] &= ~(CLIENT_COMPRESS);
	packet->str[off] &= ~(CLIENT_SSL);

	server_cap    = network_mysqld_proto_get_int16(packet, &off);

	if (server_cap & CLIENT_COMPRESS) {
		packet->str[off-2] &= ~(CLIENT_COMPRESS);
	}

	if (server_cap & CLIENT_SSL) {
		packet->str[off-1] &= ~(CLIENT_SSL >> 8);
	}

	
	server_lang   = network_mysqld_proto_get_int8(packet, &off);
	server_status = network_mysqld_proto_get_int16(packet, &off);
	
	network_mysqld_proto_skip(packet, &off, 13);
	
	scramble_2 = network_mysqld_proto_get_string_len(packet, &off, 13);

	/**
	 * scramble_1 + scramble_2 == scramble
	 *
	 * a len-encoded string
	 */

	g_string_truncate(recv_sock->scramble_buf, 0);
	g_string_append_len(recv_sock->scramble_buf, scramble_1, 8);
	g_string_append_len(recv_sock->scramble_buf, scramble_2, 13);

	g_free(scramble_1);
	g_free(scramble_2);
	
	g_string_truncate(recv_sock->auth_handshake_packet, 0);
	g_string_append_len(recv_sock->auth_handshake_packet, packet->str + NET_HEADER_SIZE, packet->len - NET_HEADER_SIZE);

	switch (proxy_lua_read_handshake(con)) {
	case PROXY_NO_DECISION:
		break;
	case PROXY_SEND_QUERY:
		/* the client overwrote and wants to send its own packet
		 * it is already in the queue */

		recv_sock->packet_len = PACKET_LEN_UNSET;
		g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

		return RET_ERROR;
	default:
		g_error("%s.%d: ...", __FILE__, __LINE__);
		break;
	} 

	/*
	 * move the packets to the server queue 
	 */
	network_queue_append_chunk(send_sock->send_queue, packet);

	recv_sock->packet_len = PACKET_LEN_UNSET;
	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	/* copy the pack to the client */
	con->state = CON_STATE_SEND_HANDSHAKE;

	return RET_SUCCESS;
}

static proxy_stmt_ret proxy_lua_read_auth(network_mysqld_con *con) {
	proxy_stmt_ret ret = PROXY_NO_DECISION;

#ifdef HAVE_LUA_H
	plugin_con_state *st = con->plugin_con_state;
	lua_State *L;

	/* call the lua script to pick a backend
	 * */
	lua_register_callback(con);

	if (!st->injected.L) return 0;

	L = st->injected.L;

	g_assert(lua_isfunction(L, -1));
	lua_getfenv(L, -1);
	g_assert(lua_istable(L, -1));
	
	lua_getfield_literal(L, -1, C("read_auth"));
	if (lua_isfunction(L, -1)) {

		/* export
		 *
		 * every thing we know about it
		 *  */

		lua_newtable(L);

		lua_pushlstring(L, con->client->username->str, con->client->username->len);
		lua_setfield(L, -2, "username");
		lua_pushlstring(L, con->client->scrambled_password->str, con->client->scrambled_password->len);
		lua_setfield(L, -2, "password");
		lua_pushlstring(L, con->client->default_db->str, con->client->default_db->len);
		lua_setfield(L, -2, "default_db");

		if (lua_pcall(L, 1, 1, 0) != 0) {
			g_critical("(read_auth) %s", lua_tostring(L, -1));

			lua_pop(L, 1); /* errmsg */

			/* the script failed, but we have a useful default */
		} else {
			if (lua_isnumber(L, -1)) {
				ret = lua_tonumber(L, -1);
			}
			lua_pop(L, 1);
		}

		switch (ret) {
		case PROXY_NO_DECISION:
			break;
		case PROXY_SEND_RESULT:
			/* answer directly */

			con->client->packet_id++;
			
			if (proxy_lua_handle_proxy_response(con)) {
				/**
				 * handling proxy.response failed
				 *
				 * send a ERR packet
				 */
		
				network_mysqld_con_send_error(con->client, C("(lua) handling proxy.response failed, check error-log"));
			}

			break;
		default:
			ret = PROXY_NO_DECISION;
			break;
		}

		/* ret should be a index into */

	} else if (lua_isnil(L, -1)) {
		lua_pop(L, 1); /* pop the nil */
	} else {
		g_message("%s.%d: %s", __FILE__, __LINE__, lua_typename(L, lua_type(L, -1)));
		lua_pop(L, 1); /* pop the ... */
	}
	lua_pop(L, 1); /* fenv */

	g_assert(lua_isfunction(L, -1));
#endif
	return ret;
}

typedef struct {
	guint32 client_flags;
	guint32 max_packet_size;
	guint8  charset_number;
	gchar * user;
	gchar * scramble_buf;
	gchar * db_name;
} mysql_packet_auth;

NETWORK_MYSQLD_PLUGIN_PROTO(proxy_read_auth) {
	/* read auth from client */
	GString *packet;
	GList *chunk;
	network_socket *recv_sock, *send_sock;
	mysql_packet_auth auth;
	guint off = 0;

	recv_sock = con->client;
	send_sock = con->server;

	chunk = recv_sock->recv_queue->chunks->tail;
	packet = chunk->data;

	if (packet->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS; /* we are not finished yet */

	/* extract the default db from it */
	network_mysqld_proto_skip(packet, &off, NET_HEADER_SIZE); /* packet-header */

	/*
	 * @\0\0\1
	 *  \215\246\3\0 - client-flags
	 *  \0\0\0\1     - max-packet-len
	 *  \10          - charset-num
	 *  \0\0\0\0
	 *  \0\0\0\0
	 *  \0\0\0\0
	 *  \0\0\0\0
	 *  \0\0\0\0
	 *  \0\0\0       - fillers
	 *  root\0       - username
	 *  \24          - len of the scrambled buf
	 *    ~    \272 \361 \346
	 *    \211 \353 D    \351
	 *    \24  \243 \223 \257
	 *    \0   ^    \n   \254
	 *    t    \347 \365 \244
	 *  
	 *  world\0
	 */

	auth.client_flags    = network_mysqld_proto_get_int32(packet, &off);
	auth.max_packet_size = network_mysqld_proto_get_int32(packet, &off);
	auth.charset_number  = network_mysqld_proto_get_int8(packet, &off);

	network_mysqld_proto_skip(packet, &off, 23);
	
	network_mysqld_proto_get_gstring(packet, &off, con->client->username);
	network_mysqld_proto_get_lenenc_gstring(packet, &off, con->client->scrambled_password);

	if (off != packet->len) {
		network_mysqld_proto_get_gstring(packet, &off, con->client->default_db);
	}

	/**
	 * looks like we finished parsing, call the lua function
	 */

	switch (proxy_lua_read_auth(con)) {
	case PROXY_SEND_RESULT:
		con->state = CON_STATE_SEND_AUTH_RESULT;

		g_string_free(packet, TRUE);
		chunk->data = packet = NULL;

		break;
	case PROXY_NO_DECISION:
		if (con->server->is_authed) {
			GString *com_change_user = g_string_new(NULL);
			/* copy incl. the nul */
			g_string_append_c(com_change_user, COM_CHANGE_USER);
			g_string_append_len(com_change_user, con->client->username->str, con->client->username->len + 1);

			g_assert(con->client->scrambled_password->len < 250);

			g_string_append_c(com_change_user, (con->client->scrambled_password->len & 0xff));
			g_string_append_len(com_change_user, con->client->scrambled_password->str, con->client->scrambled_password->len);

			g_string_append_len(com_change_user, con->client->default_db->str, con->client->default_db->len + 1);
			
			network_queue_append(send_sock->send_queue, 
					com_change_user->str, 
					com_change_user->len, 
					0);

			/**
			 * the server is already authenticated, the client isn't
			 *
			 * transform the auth-packet into a COM_CHANGE_USER
			 */

			g_string_free(com_change_user, TRUE);

			/**
			 * the packet isn't appended anywhere, free it
			 */
			g_string_free(packet, TRUE);
			chunk->data = packet = NULL;
		} else {
			network_queue_append_chunk(send_sock->send_queue, packet);
		}

		con->state = CON_STATE_SEND_AUTH;
		break;
	default:
		g_error("%s.%d: ... ", __FILE__, __LINE__);
		break;
	}

	recv_sock->packet_len = PACKET_LEN_UNSET;
	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	return RET_SUCCESS;
}

static proxy_stmt_ret proxy_lua_read_auth_result(network_mysqld_con *con) {
	proxy_stmt_ret ret = PROXY_NO_DECISION;

#ifdef HAVE_LUA_H
	plugin_con_state *st = con->plugin_con_state;
	network_socket *recv_sock = con->server;
	GList *chunk = recv_sock->recv_queue->chunks->tail;
	GString *packet = chunk->data;
	lua_State *L;

	/* call the lua script to pick a backend
	 * */
	lua_register_callback(con);

	if (!st->injected.L) return 0;

	L = st->injected.L;

	g_assert(lua_isfunction(L, -1));
	lua_getfenv(L, -1);
	g_assert(lua_istable(L, -1));
	
	lua_getfield_literal(L, -1, C("read_auth_result"));
	if (lua_isfunction(L, -1)) {

		/* export
		 *
		 * every thing we know about it
		 *  */

		lua_newtable(L);

		lua_pushlstring(L, packet->str + NET_HEADER_SIZE, packet->len - NET_HEADER_SIZE);
		lua_setfield(L, -2, "packet");

		if (lua_pcall(L, 1, 1, 0) != 0) {
			g_critical("(read_auth_result) %s", lua_tostring(L, -1));

			lua_pop(L, 1); /* errmsg */

			/* the script failed, but we have a useful default */
		} else {
			if (lua_isnumber(L, -1)) {
				ret = lua_tonumber(L, -1);
			}
			lua_pop(L, 1);
		}

		switch (ret) {
		case PROXY_NO_DECISION:
			break;
		case PROXY_SEND_RESULT:
			/* answer directly */

			if (proxy_lua_handle_proxy_response(con)) {
				/**
				 * handling proxy.response failed
				 *
				 * send a ERR packet
				 */
		
				network_mysqld_con_send_error(con->client, C("(lua) handling proxy.response failed, check error-log"));
			}

			break;
		default:
			ret = PROXY_NO_DECISION;
			break;
		}

		/* ret should be a index into */

	} else if (lua_isnil(L, -1)) {
		lua_pop(L, 1); /* pop the nil */
	} else {
		g_message("%s.%d: %s", __FILE__, __LINE__, lua_typename(L, lua_type(L, -1)));
		lua_pop(L, 1); /* pop the ... */
	}
	lua_pop(L, 1); /* fenv */

	g_assert(lua_isfunction(L, -1));
#endif
	return ret;
}


NETWORK_MYSQLD_PLUGIN_PROTO(proxy_read_auth_result) {
	GString *packet;
	GList *chunk;
	network_socket *recv_sock, *send_sock;

	recv_sock = con->server;
	send_sock = con->client;

	chunk = recv_sock->recv_queue->chunks->tail;
	packet = chunk->data;

	/* we aren't finished yet */
	if (packet->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS;

	/* send the auth result to the client */
	if (con->server->is_authed) {
		/**
		 * we injected a COM_CHANGE_USER above and have to correct to 
		 * packet-id now 
		 */
		packet->str[3] = 2;
	}

	/**
	 * copy the 
	 * - default-db, 
	 * - username, 
	 * - scrambed_password
	 *
	 * to the server-side 
	 */
	g_string_truncate(recv_sock->username, 0);
	g_string_append_len(recv_sock->username, send_sock->username->str, send_sock->username->len);
	g_string_truncate(recv_sock->default_db, 0);
	g_string_append_len(recv_sock->default_db, send_sock->default_db->str, send_sock->default_db->len);
	g_string_truncate(recv_sock->scrambled_password, 0);
	g_string_append_len(recv_sock->scrambled_password, send_sock->scrambled_password->str, send_sock->scrambled_password->len);

	/**
	 * recv_sock still points to the old backend that
	 * we received the packet from. 
	 *
	 * backend_ndx = 0 might have reset con->server
	 */

	switch (proxy_lua_read_auth_result(con)) {
	case PROXY_SEND_RESULT:
		/**
		 * we already have content in the send-sock 
		 *
		 * chunk->packet is not forwarded, free it
		 */

		g_string_free(packet, TRUE);
		
		break;
	case PROXY_NO_DECISION:
		network_queue_append_chunk(send_sock->send_queue, packet);

		break;
	default:
		g_error("%s.%d: ... ", __FILE__, __LINE__);
		break;
	}

	/**
	 * we handled the packet on the server side, free it
	 */
	recv_sock->packet_len = PACKET_LEN_UNSET;
	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);
	
	con->state = CON_STATE_SEND_AUTH_RESULT;

	return RET_SUCCESS;
}

static proxy_stmt_ret proxy_lua_read_query(network_mysqld_con *con) {
	plugin_con_state *st = con->plugin_con_state;
	char command = -1;
	injection *inj;
	network_socket *recv_sock = con->client;
	GList   *chunk  = recv_sock->recv_queue->chunks->head;
	GString *packet = chunk->data;
	chassis_plugin_config *config = con->config;

	if (!config->profiling) return PROXY_SEND_QUERY;

	if (packet->len < NET_HEADER_SIZE) return PROXY_SEND_QUERY; /* packet too short */

	command = packet->str[NET_HEADER_SIZE + 0];

	if (COM_QUERY == command) {
		/* we need some more data after the COM_QUERY */
		if (packet->len < NET_HEADER_SIZE + 2) return PROXY_SEND_QUERY;

		/* LOAD DATA INFILE is nasty */
		if (packet->len - NET_HEADER_SIZE - 1 >= sizeof("LOAD ") - 1 &&
		    0 == g_ascii_strncasecmp(packet->str + NET_HEADER_SIZE + 1, C("LOAD "))) return PROXY_SEND_QUERY;

		/* don't cover them with injected queries as it trashes the result */
		if (packet->len - NET_HEADER_SIZE - 1 >= sizeof("SHOW ERRORS") - 1 &&
		    0 == g_ascii_strncasecmp(packet->str + NET_HEADER_SIZE + 1, C("SHOW ERRORS"))) return PROXY_SEND_QUERY;
		if (packet->len - NET_HEADER_SIZE - 1 >= sizeof("select @@error_count") - 1 &&
		    0 == g_ascii_strncasecmp(packet->str + NET_HEADER_SIZE + 1, C("select @@error_count"))) return PROXY_SEND_QUERY;
	
	}

	/* reset the query status */
	memset(&(st->injected.qstat), 0, sizeof(st->injected.qstat));
	
	while ((inj = g_queue_pop_head(st->injected.queries))) injection_free(inj);

	/* ok, here we go */

#ifdef HAVE_LUA_H
	lua_register_callback(con);

	if (st->injected.L) {
		lua_State *L = st->injected.L;
		proxy_stmt_ret ret = PROXY_NO_DECISION;

		g_assert(lua_isfunction(L, -1));
		lua_getfenv(L, -1);
		g_assert(lua_istable(L, -1));

		/**
		 * reset proxy.response to a empty table 
		 */
		lua_getfield(L, -1, "proxy");
		g_assert(lua_istable(L, -1));

		lua_newtable(L);
		lua_setfield(L, -2, "response");

		lua_pop(L, 1);
		
		/**
		 * get the call back
		 */
		lua_getfield_literal(L, -1, C("read_query"));
		if (lua_isfunction(L, -1)) {

			/* pass the packet as parameter */
			lua_pushlstring(L, packet->str + NET_HEADER_SIZE, packet->len - NET_HEADER_SIZE);

			if (lua_pcall(L, 1, 1, 0) != 0) {
				/* hmm, the query failed */
				g_critical("(read_query) %s", lua_tostring(L, -1));

				lua_pop(L, 2); /* fenv + errmsg */

				/* perhaps we should clean up ?*/

				return PROXY_SEND_QUERY;
			} else {
				if (lua_isnumber(L, -1)) {
					ret = lua_tonumber(L, -1);
				}
				lua_pop(L, 1);
			}

			switch (ret) {
			case PROXY_SEND_RESULT:
				/* check the proxy.response table for content,
				 *
				 */
	
				con->client->packet_id++;

				if (proxy_lua_handle_proxy_response(con)) {
					/**
					 * handling proxy.response failed
					 *
					 * send a ERR packet
					 */
			
					network_mysqld_con_send_error(con->client, C("(lua) handling proxy.response failed, check error-log"));
				}
	
				break;
			case PROXY_NO_DECISION:
				/**
				 * PROXY_NO_DECISION and PROXY_SEND_QUERY may pick another backend
				 */
				break;
			case PROXY_SEND_QUERY:
				/* send the injected queries
				 *
				 * injection_init(..., query);
				 * 
				 *  */

				if (st->injected.queries->length) {
					ret = PROXY_SEND_INJECTION;
				}
	
				break;
			default:
				break;
			}
			lua_pop(L, 1); /* fenv */
		} else {
			lua_pop(L, 2); /* fenv + nil */
		}

		g_assert(lua_isfunction(L, -1));

		if (ret != PROXY_NO_DECISION) {
			return ret;
		}
	}
#endif
	return PROXY_NO_DECISION;
}

/**
 * gets called after a query has been read
 *
 * - calls the lua script via network_mysqld_con_handle_proxy_stmt()
 *
 * @see network_mysqld_con_handle_proxy_stmt
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_read_query) {
	GString *packet;
	GList *chunk;
	network_socket *recv_sock, *send_sock;
	plugin_con_state *st = con->plugin_con_state;
	int proxy_query = 1;
	proxy_stmt_ret ret;

	send_sock = NULL;
	recv_sock = con->client;
	st->injected.sent_resultset = 0;

	chunk = recv_sock->recv_queue->chunks->head;

	if (recv_sock->recv_queue->chunks->length != 1) {
		g_message("%s.%d: client-recv-queue-len = %d", __FILE__, __LINE__, recv_sock->recv_queue->chunks->length);
	}
	
	packet = chunk->data;

	if (packet->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS;

	con->parse.len = recv_sock->packet_len;

	ret = proxy_lua_read_query(con);

	/**
	 * if we disconnected in read_query_result() we have no connection open
	 * when we try to execute the next query 
	 *
	 * for PROXY_SEND_RESULT we don't need a server
	 */
	if (ret != PROXY_SEND_RESULT &&
	    con->server == NULL) {
		g_critical("%s.%d: I have no server backend, closing connection", __FILE__, __LINE__);
		return RET_ERROR;
	}
	
	send_sock = con->server;

	switch (ret) {
	case PROXY_NO_DECISION:
	case PROXY_SEND_QUERY:
		/* no injection, pass on the chunk as is */
		send_sock->packet_id = recv_sock->packet_id;

		network_queue_append_chunk(send_sock->send_queue, packet);

		break;
	case PROXY_SEND_RESULT: 
		proxy_query = 0;
		
		g_string_free(chunk->data, TRUE);

		break; 
	case PROXY_SEND_INJECTION: {
		injection *inj;

		inj = g_queue_peek_head(st->injected.queries);

		/* there might be no query, if it was banned */
		network_queue_append(send_sock->send_queue, inj->query->str, inj->query->len, 0);

		g_string_free(chunk->data, TRUE);

		break; }
	default:
		g_error("%s.%d: ", __FILE__, __LINE__);
	}

	recv_sock->packet_len = PACKET_LEN_UNSET;
	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	if (proxy_query) {
		con->state = CON_STATE_SEND_QUERY;
	} else {
		con->state = CON_STATE_SEND_QUERY_RESULT;
	}

	return RET_SUCCESS;
}

/**
 * decide about the next state after the result-set has been written 
 * to the client
 * 
 * if we still have data in the queue, back to proxy_send_query()
 * otherwise back to proxy_read_query() to pick up a new client query
 *
 * @note we should only send one result back to the client
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_send_query_result) {
	network_socket *recv_sock, *send_sock;
	injection *inj;
	plugin_con_state *st = con->plugin_con_state;

	send_sock = con->server;
	recv_sock = con->client;

	if (st->connection_close) {
		con->state = CON_STATE_ERROR;

		return RET_SUCCESS;
	}

	if (con->parse.command == COM_BINLOG_DUMP) {
		/**
		 * the binlog dump is different as it doesn't have END packet
		 *
		 * @todo in 5.0.x a NON_BLOCKING option as added which sends a EOF
		 */
		con->state = CON_STATE_READ_QUERY_RESULT;

		return RET_SUCCESS;
	}

	if (st->injected.queries->length == 0) {
		con->state = CON_STATE_READ_QUERY;

		return RET_SUCCESS;
	}

	con->parse.len = recv_sock->packet_len;

	inj = g_queue_peek_head(st->injected.queries);

	network_queue_append(send_sock->send_queue, inj->query->str, inj->query->len, 0);

	con->state = CON_STATE_SEND_QUERY;

	return RET_SUCCESS;
}

/**
 * handle the query-result we received from the server
 *
 * - decode the result-set to track if we are finished already
 * - handles BUG#25371 if requested
 * - if the packet is finished, calls the network_mysqld_con_handle_proxy_resultset
 *   to handle the resultset in the lua-scripts
 *
 * @see network_mysqld_con_handle_proxy_resultset
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_read_query_result) {
	int is_finished = 0;
	int send_packet = 1; /* shall we forward this packet ? */
	GString *packet;
	GList *chunk;
	network_socket *recv_sock, *send_sock;
	plugin_con_state *st = con->plugin_con_state;
	injection *inj = NULL;
	chassis_plugin_config *config = con->config;

	recv_sock = con->server;
	send_sock = con->client;

	chunk = recv_sock->recv_queue->chunks->tail;
	packet = chunk->data;

	/**
	 * check if we want to forward the statement to the client 
	 *
	 * if not, clean the send-queue 
	 */

	if (0 != st->injected.queries->length) {
		inj = g_queue_peek_head(st->injected.queries);
	}

	if (inj && inj->ts_read_query_result_first.tv_sec == 0) {
		/**
		 * log the time of the first received packet
		 */
		g_get_current_time(&(inj->ts_read_query_result_first));
	}

	if (packet->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS;

#if 0
	g_message("%s.%d: packet-len: %08x, packet-id: %d, command: COM_(%02x)", 
			__FILE__, __LINE__,
			recv_sock->packet_len,
			recv_sock->packet_id,
			con->parse.command
		);
#endif						
	/* forward the response to the client */
	switch (con->parse.command) {
	case COM_CHANGE_USER:
		/**
		 * - OK
		 * - ERR (in 5.1.12+ + a duplicate ERR)
		 */
		switch (packet->str[NET_HEADER_SIZE + 0]) {
		case MYSQLD_PACKET_ERR:
			if (recv_sock->mysqld_version > 50113 && recv_sock->mysqld_version < 50118) {
				/**
				 * Bug #25371
				 *
				 * COM_CHANGE_USER returns 2 ERR packets instead of one
				 *
				 * we can auto-correct the issue if needed and remove the second packet
				 * Some clients handle this issue and expect a double ERR packet.
				 */
				if (recv_sock->packet_id == 2) {
					if (config->fix_bug_25371) {
						send_packet = 0;
					}
					is_finished = 1;
				}
			} else {
				is_finished = 1;
			}
			break;
		case MYSQLD_PACKET_OK:
			is_finished = 1;
			break;
		default:
			g_error("%s.%d: COM_(0x%02x) should be (ERR|OK), got %02x",
					__FILE__, __LINE__,
					con->parse.command, packet->str[0 + NET_HEADER_SIZE]);
			break;
		}
		break;
	case COM_INIT_DB:
		/**
		 * in case we have a init-db statement we track the db-change on the server-side
		 * connection
		 */
		switch (packet->str[NET_HEADER_SIZE + 0]) {
		case MYSQLD_PACKET_ERR:
			is_finished = 1;
			break;
		case MYSQLD_PACKET_OK:
			/**
			 * track the change of the init_db */
			g_string_truncate(con->server->default_db, 0);
			g_string_truncate(con->client->default_db, 0);
			if (con->parse.state.init_db.db_name->len) {
				g_string_append_len(con->server->default_db, 
						con->parse.state.init_db.db_name->str,
						con->parse.state.init_db.db_name->len);
				
				g_string_append_len(con->client->default_db, 
						con->parse.state.init_db.db_name->str,
						con->parse.state.init_db.db_name->len);
			}
			 
			is_finished = 1;
			break;
		default:
			g_error("%s.%d: COM_(0x%02x) should be (ERR|OK), got %02x",
					__FILE__, __LINE__,
					con->parse.command, packet->str[0 + NET_HEADER_SIZE]);
			break;
		}

		break;
	case COM_STMT_RESET:
	case COM_PING:
	case COM_PROCESS_KILL:
		switch (packet->str[NET_HEADER_SIZE + 0]) {
		case MYSQLD_PACKET_ERR:
		case MYSQLD_PACKET_OK:
			is_finished = 1;
			break;
		default:
			g_error("%s.%d: COM_(0x%02x) should be (ERR|OK), got %02x",
					__FILE__, __LINE__,
					con->parse.command, packet->str[0 + NET_HEADER_SIZE]);
			break;
		}
		break;
	case COM_DEBUG:
	case COM_SET_OPTION:
	case COM_SHUTDOWN:
		switch (packet->str[NET_HEADER_SIZE + 0]) {
		case MYSQLD_PACKET_EOF:
			is_finished = 1;
			break;
		default:
			g_error("%s.%d: COM_(0x%02x) should be EOF, got %02x",
					__FILE__, __LINE__,
					con->parse.command, packet->str[0 + NET_HEADER_SIZE]);
			break;
		}
		break;

	case COM_FIELD_LIST:
		/* we transfer some data and wait for the EOF */
		switch (packet->str[NET_HEADER_SIZE + 0]) {
		case MYSQLD_PACKET_ERR:
		case MYSQLD_PACKET_EOF:
			is_finished = 1;
			break;
		case MYSQLD_PACKET_NULL:
		case MYSQLD_PACKET_OK:
			g_error("%s.%d: COM_(0x%02x), packet %d should not be (OK|ERR|NULL), got: %02x",
					__FILE__, __LINE__,
					con->parse.command, recv_sock->packet_id, packet->str[NET_HEADER_SIZE + 0]);

			break;
		default:
			break;
		}
		break;
#if MYSQL_VERSION_ID >= 50000
	case COM_STMT_FETCH:
		/*  */
		switch (packet->str[NET_HEADER_SIZE + 0]) {
		case MYSQLD_PACKET_EOF:
			if (packet->str[NET_HEADER_SIZE + 3] & SERVER_STATUS_LAST_ROW_SENT) {
				is_finished = 1;
			}
			if (packet->str[NET_HEADER_SIZE + 3] & SERVER_STATUS_CURSOR_EXISTS) {
				is_finished = 1;
			}
			break;
		default:
			break;
		}
		break;
#endif
	case COM_QUIT: /* sometimes we get a packet before the connection closes */
	case COM_STATISTICS:
		/* just one packet, no EOF */
		is_finished = 1;

		break;
	case COM_STMT_PREPARE:
		if (con->parse.state.prepare.first_packet == 1) {
			con->parse.state.prepare.first_packet = 0;

			switch (packet->str[NET_HEADER_SIZE + 0]) {
			case MYSQLD_PACKET_OK:
				g_assert(packet->len == 12 + NET_HEADER_SIZE); 

				/* the header contains the number of EOFs we expect to see
				 * - no params -> 0
				 * - params | fields -> 1
				 * - params + fields -> 2 
				 */
				con->parse.state.prepare.want_eofs = 0;

				if (packet->str[NET_HEADER_SIZE + 5] != 0 || packet->str[NET_HEADER_SIZE + 6] != 0) {
					con->parse.state.prepare.want_eofs++;
				}
				if (packet->str[NET_HEADER_SIZE + 7] != 0 || packet->str[NET_HEADER_SIZE + 8] != 0) {
					con->parse.state.prepare.want_eofs++;
				}

				if (con->parse.state.prepare.want_eofs == 0) {
					is_finished = 1;
				}

				break;
			case MYSQLD_PACKET_ERR:
				is_finished = 1;
				break;
			default:
				g_error("%s.%d: COM_(0x%02x) should either get a (OK|ERR), got %02x",
						__FILE__, __LINE__,
						con->parse.command, packet->str[NET_HEADER_SIZE + 0]);
				break;
			}
		} else {
			switch (packet->str[NET_HEADER_SIZE + 0]) {
			case MYSQLD_PACKET_OK:
			case MYSQLD_PACKET_NULL:
			case MYSQLD_PACKET_ERR:
				g_error("%s.%d: COM_(0x%02x), packet %d should not be (OK|ERR|NULL), got: %02x",
						__FILE__, __LINE__,
						con->parse.command, recv_sock->packet_id, packet->str[NET_HEADER_SIZE + 0]);
				break;
			case MYSQLD_PACKET_EOF:
				if (--con->parse.state.prepare.want_eofs == 0) {
					is_finished = 1;
				}
				break;
			default:
				break;
			}
		}

		break;
	case COM_STMT_EXECUTE:
	case COM_QUERY:
		/**
		 * if we get a OK in the first packet there will be no result-set
		 */
		switch (con->parse.state.query) {
		case PARSE_COM_QUERY_INIT:
			switch (packet->str[NET_HEADER_SIZE + 0]) {
			case MYSQLD_PACKET_ERR: /* e.g. SELECT * FROM dual -> ERROR 1096 (HY000): No tables used */
				g_assert(con->parse.state.query == PARSE_COM_QUERY_INIT);

				is_finished = 1;
				break;
			case MYSQLD_PACKET_OK: { /* e.g. DELETE FROM tbl */
				int server_status;
				int warning_count;
				guint64 affected_rows;
				guint64 insert_id;
				GString s;

				s.str = packet->str + NET_HEADER_SIZE;
				s.len = packet->len - NET_HEADER_SIZE;

				network_mysqld_proto_get_ok_packet(&s, &affected_rows, &insert_id, &server_status, &warning_count, NULL);
				if (server_status & SERVER_MORE_RESULTS_EXISTS) {
				
				} else {
					is_finished = 1;
				}

				st->injected.qstat.server_status = server_status;
				st->injected.qstat.warning_count = warning_count;
				st->injected.qstat.affected_rows = affected_rows;
				st->injected.qstat.insert_id     = insert_id;
				st->injected.qstat.was_resultset = 0;

				break; }
			case MYSQLD_PACKET_NULL:
				/* OH NO, LOAD DATA INFILE :) */
				con->parse.state.query = PARSE_COM_QUERY_LOAD_DATA;

				is_finished = 1;

				break;
			case MYSQLD_PACKET_EOF:
				g_error("%s.%d: COM_(0x%02x), packet %d should not be (NULL|EOF), got: %02x",
						__FILE__, __LINE__,
						con->parse.command, recv_sock->packet_id, packet->str[NET_HEADER_SIZE + 0]);

				break;
			default:
				/* looks like a result */
				con->parse.state.query = PARSE_COM_QUERY_FIELD;
				break;
			}
			break;
		case PARSE_COM_QUERY_FIELD:
			switch (packet->str[NET_HEADER_SIZE + 0]) {
			case MYSQLD_PACKET_ERR:
			case MYSQLD_PACKET_OK:
			case MYSQLD_PACKET_NULL:
				g_error("%s.%d: COM_(0x%02x), packet %d should not be (OK|NULL|ERR), got: %02x",
						__FILE__, __LINE__,
						con->parse.command, recv_sock->packet_id, packet->str[NET_HEADER_SIZE + 0]);

				break;
			case MYSQLD_PACKET_EOF:
#if MYSQL_VERSION_ID >= 50000
				/**
				 * in 5.0 we have CURSORs which have no rows, just a field definition
				 */
				if (packet->str[NET_HEADER_SIZE + 3] & SERVER_STATUS_CURSOR_EXISTS) {
					is_finished = 1;
				} else {
					con->parse.state.query = PARSE_COM_QUERY_RESULT;
				}
#else
				con->parse.state.query = PARSE_COM_QUERY_RESULT;
#endif
				break;
			default:
				break;
			}
			break;
		case PARSE_COM_QUERY_RESULT:
			switch (packet->str[NET_HEADER_SIZE + 0]) {
			case MYSQLD_PACKET_EOF:
				if (recv_sock->packet_len < 9) {
					/* so much on the binary-length-encoding 
					 *
					 * sometimes the len-encoding is ...
					 *
					 * */

					if (packet->str[NET_HEADER_SIZE + 3] & SERVER_MORE_RESULTS_EXISTS) {
						con->parse.state.query = PARSE_COM_QUERY_INIT;
					} else {
						is_finished = 1;
					}

					st->injected.qstat.server_status = packet->str[NET_HEADER_SIZE + 3] | (packet->str[NET_HEADER_SIZE + 4] >> 8);
					st->injected.qstat.warning_count = packet->str[NET_HEADER_SIZE + 1] | (packet->str[NET_HEADER_SIZE + 2] >> 8);

					st->injected.qstat.was_resultset = 1;
				}

				break;
			case MYSQLD_PACKET_ERR:
				/* like 
				 * 
				 * EXPLAIN SELECT * FROM dual; returns an error
				 * 
				 * EXPLAIN SELECT 1 FROM dual; returns a result-set
				 * */
				is_finished = 1;
				break;
			case MYSQLD_PACKET_OK:
			case MYSQLD_PACKET_NULL: /* the first field might be a NULL */
				break;
			default:
				break;
			}
			break;
		case PARSE_COM_QUERY_LOAD_DATA_END_DATA:
			switch (packet->str[NET_HEADER_SIZE + 0]) {
			case MYSQLD_PACKET_OK:
				is_finished = 1;
				break;
			case MYSQLD_PACKET_NULL:
			case MYSQLD_PACKET_ERR:
			case MYSQLD_PACKET_EOF:
			default:
				g_error("%s.%d: COM_(0x%02x), packet %d should be (OK), got: %02x",
						__FILE__, __LINE__,
						con->parse.command, recv_sock->packet_id, packet->str[NET_HEADER_SIZE + 0]);


				break;
			}

			break;
		default:
			g_error("%s.%d: unknown state in COM_(0x%02x): %d", 
					__FILE__, __LINE__,
					con->parse.command,
					con->parse.state.query);
		}
		break;
	case COM_BINLOG_DUMP:
		/**
		 * the binlog-dump event stops, forward all packets as we see them
		 * and keep the command active
		 */
		is_finished = 1;
		break;
	default:
		g_error("%s.%d: COM_(0x%02x) is not handled", 
				__FILE__, __LINE__,
				con->parse.command);
		break;
	}

	if (send_packet) {
		network_queue_append_chunk(send_sock->send_queue, packet);
	} else {
		if (chunk->data) g_string_free(chunk->data, TRUE);
	}

	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);
	recv_sock->packet_len = PACKET_LEN_UNSET;

	if (is_finished) {
		/**
		 * the resultset handler might decide to trash the send-queue
		 * 
		 * */

		if (inj) {
			g_get_current_time(&(inj->ts_read_query_result_last));
		}

		proxy_lua_read_query_result(con);

		/** recv_sock might be != con->server now */

		/**
		 * if the send-queue is empty, we have nothing to send
		 * and can read the next query */	
		if (send_sock->send_queue->chunks) {
			con->state = CON_STATE_SEND_QUERY_RESULT;
		} else {
			con->state = CON_STATE_READ_QUERY;
		}
	}
	
	return RET_SUCCESS;
}

#ifdef HAVE_LUA_H
/**
 * dump the content of a lua table
 */
static void proxy_lua_dumptable(lua_State *L) {
	g_assert(lua_istable(L, -1));

	lua_pushnil(L);
	while (lua_next(L, -2) != 0) {
		int t = lua_type(L, -2);

		switch (t) {
		case LUA_TSTRING:
			g_message("[%d] (string) %s", 0, lua_tostring(L, -2));
			break;
		case LUA_TBOOLEAN:
			g_message("[%d] (bool) %s", 0, lua_toboolean(L, -2) ? "true" : "false");
			break;
		case LUA_TNUMBER:
			g_message("[%d] (number) %g", 0, lua_tonumber(L, -2));
			break;
		default:
			g_message("[%d] (%s)", 0, lua_typename(L, lua_type(L, -2)));
			break;
		}
		g_message("[%d] (%s)", 0, lua_typename(L, lua_type(L, -1)));

		lua_pop(L, 1);
	}
}
#endif

static proxy_stmt_ret proxy_lua_connect_server(network_mysqld_con *con) {
	proxy_stmt_ret ret = PROXY_NO_DECISION;

#ifdef HAVE_LUA_H
	plugin_con_state *st = con->plugin_con_state;
	lua_State *L;

	/* call the lua script to pick a backend
	 * */
	lua_register_callback(con);

	if (!st->injected.L) return 0;

	L = st->injected.L;

	g_assert(lua_isfunction(L, -1));
	lua_getfenv(L, -1);
	g_assert(lua_istable(L, -1));
	
	lua_getfield_literal(L, -1, C("connect_server"));
	if (lua_isfunction(L, -1)) {
		if (lua_pcall(L, 0, 1, 0) != 0) {
			g_critical("%s: (connect_server) %s", 
					G_STRLOC,
					lua_tostring(L, -1));

			lua_pop(L, 1); /* errmsg */

			/* the script failed, but we have a useful default */
		} else {
			if (lua_isnumber(L, -1)) {
				ret = lua_tonumber(L, -1);
			}
			lua_pop(L, 1);
		}

		switch (ret) {
		case PROXY_NO_DECISION:
		case PROXY_IGNORE_RESULT:
			break;
		case PROXY_SEND_RESULT:
			/* answer directly */

			if (proxy_lua_handle_proxy_response(con)) {
				/**
				 * handling proxy.response failed
				 *
				 * send a ERR packet
				 */
		
				network_mysqld_con_send_error(con->client, C("(lua) handling proxy.response failed, check error-log"));
			}

			break;
		default:
			ret = PROXY_NO_DECISION;
			break;
		}

		/* ret should be a index into */

	} else if (lua_isnil(L, -1)) {
		lua_pop(L, 1); /* pop the nil */
	} else {
		g_message("%s.%d: %s", __FILE__, __LINE__, lua_typename(L, lua_type(L, -1)));
		lua_pop(L, 1); /* pop the ... */
	}
	lua_pop(L, 1); /* fenv */

	g_assert(lua_isfunction(L, -1));
#endif
	return ret;
}



/**
 * connect to a backend
 *
 * @return
 *   RET_SUCCESS        - connected successfully
 *   RET_ERROR_RETRY    - connecting backend failed, call again to connect to another backend
 *   RET_ERROR          - no backends available, adds a ERR packet to the client queue
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_connect_server) {
	plugin_con_state *st = con->plugin_con_state;
	plugin_srv_state *g = st->global_state;
	guint min_connected_clients = G_MAXUINT;
	guint i;
	GTimeVal now;
	gboolean use_pooled_connection = FALSE;

	if (con->server) {
		int so_error = 0;
		socklen_t so_error_len = sizeof(so_error);

		/**
		 * we might get called a 2nd time after a connect() == EINPROGRESS
		 */
		if (getsockopt(con->server->fd, SOL_SOCKET, SO_ERROR, &so_error, &so_error_len)) {
			/* getsockopt failed */
			g_critical("%s.%d: getsockopt(%s) failed: %s", 
					__FILE__, __LINE__,
					con->server->addr.str, strerror(errno));
			return RET_ERROR;
		}

		switch (so_error) {
		case 0:
			break;
		default:
			g_critical("%s.%d: connect(%s) failed: %s", 
					__FILE__, __LINE__,
					con->server->addr.str, strerror(so_error));
			return RET_ERROR;
		}

		if (st->backend->state != BACKEND_STATE_UP) {
			st->backend->state = BACKEND_STATE_UP;
			g_get_current_time(&(st->backend->state_since));
		}

		con->state = CON_STATE_READ_HANDSHAKE;

		return RET_SUCCESS;
	}

	st->backend = NULL;
	st->backend_ndx = -1;

	g_get_current_time(&now);

	if (now.tv_sec - g->backend_last_check.tv_sec > 1) {
		/* check once a second if we have to wakeup a connection */
		for (i = 0; i < g->backend_pool->len; i++) {
			backend_t *cur = g->backend_pool->pdata[i];

			if (cur->state != BACKEND_STATE_DOWN) continue;

			/* check if a backend is marked as down for more than 10 sec */

			if (now.tv_sec - cur->state_since.tv_sec > 4) {
				g_debug("%s.%d: backend %s was down for more than 10 sec, waking it up", 
						__FILE__, __LINE__,
						cur->addr.str);

				cur->state = BACKEND_STATE_UNKNOWN;
				cur->state_since = now;
			}
		}
	}

	switch (proxy_lua_connect_server(con)) {
	case PROXY_SEND_RESULT:
		/* we answered directly ... like denial ...
		 *
		 * for sure we have something in the send-queue 
		 *  */
		
		return RET_SUCCESS;
	case PROXY_NO_DECISION:
		/* just go on */

		break;
	case PROXY_IGNORE_RESULT:
		use_pooled_connection = TRUE;

		break;
	default:
		g_error("%s.%d: ... ", __FILE__, __LINE__);
		break;
	}

	/**
	 * if the current backend is down, ignore it 
	 */
	if (st->backend_ndx >= 0 && 
	    st->backend_ndx < g->backend_pool->len) {
		backend_t *cur = g->backend_pool->pdata[st->backend_ndx];

		if (cur->state == BACKEND_STATE_DOWN) {
			st->backend_ndx = -1;
		}
	}

	if (con->server && !use_pooled_connection) {
		gint bndx = st->backend_ndx;
		/* we already have a connection assigned, 
		 * but the script said we don't want to use it
		 */

		proxy_connection_pool_add_connection(con);

		st->backend_ndx = bndx;
	}

	if (st->backend_ndx < 0) {
		/**
		 * we can choose between different back addresses 
		 *
		 * prefer SQF (shorted queue first) to load all backends equally
		 */ 

		for (i = 0; i < g->backend_pool->len; i++) {
			backend_t *cur = g->backend_pool->pdata[i];
	
			/**
			 * skip backends which are down or not writable
			 */	
			if (cur->state == BACKEND_STATE_DOWN ||
			    cur->type != BACKEND_TYPE_RW) continue;
	
			if (cur->connected_clients < min_connected_clients) {
				st->backend_ndx = i;
				min_connected_clients = cur->connected_clients;
			}
		}
	
		if (st->backend_ndx >= 0 && 
		    st->backend_ndx < g->backend_pool->len) {
			st->backend = g->backend_pool->pdata[st->backend_ndx];
		}
	} else if (NULL == st->backend &&
		   st->backend_ndx >= 0 && 
		   st->backend_ndx < g->backend_pool->len) {
		st->backend = g->backend_pool->pdata[st->backend_ndx];
	}

	if (NULL == st->backend) {
		network_mysqld_con_send_error(con->client, C("(proxy) all backends are down"));
		return RET_ERROR;
	}

	/**
	 * check if we have a connection in the pool for this backend
	 */
	if (NULL == con->server) {
		con->server = network_socket_init();
		con->server->addr = st->backend->addr;
		con->server->addr.str = g_strdup(st->backend->addr.str);
	
		st->backend->connected_clients++;

		switch(network_mysqld_con_connect(con->server)) {
		case -2:
			/* the socket is non-blocking already, 
			 * call getsockopt() to see if we are done */
			return RET_ERROR_RETRY;
		case 0:
			break;
		default:
			g_message("%s.%d: connecting to backend (%s) failed, marking it as down for ...", 
					__FILE__, __LINE__, con->server->addr.str);

			st->backend->state = BACKEND_STATE_DOWN;
			g_get_current_time(&(st->backend->state_since));

			network_socket_free(con->server);
			con->server = NULL;

			return RET_ERROR_RETRY;
		}

		if (st->backend->state != BACKEND_STATE_UP) {
			st->backend->state = BACKEND_STATE_UP;
			g_get_current_time(&(st->backend->state_since));
		}

		con->state = CON_STATE_READ_HANDSHAKE;
	} else {
		/**
		 * send the old hand-shake packet
		 */

		/* remove the idle-handler from the socket */
		network_queue_append(con->client->send_queue, 
				con->server->auth_handshake_packet->str, 
				con->server->auth_handshake_packet->len,
			       	0); /* packet-id */
		
		con->state = CON_STATE_SEND_HANDSHAKE;

		/**
		 * connect_clients is already incremented 
		 */
	}

	return RET_SUCCESS;
}


plugin_srv_state *plugin_srv_state_get(chassis_plugin_config *config) {
	static plugin_srv_state *global_state = NULL;
	guint i;

	/**
	 * the global pool is started once 
	 */

	if (global_state) return global_state;
	/* if config is not set, return the old global-state (used at shutdown) */
	if (!config) return global_state;

	global_state = plugin_srv_state_init();
		
	/* init the pool */
	for (i = 0; config->backend_addresses[i]; i++) {
		backend_t *backend;
		gchar *address = config->backend_addresses[i];

		backend = backend_init();
		backend->type = BACKEND_TYPE_RW;

		if (0 != network_mysqld_con_set_address(&backend->addr, address)) {
			return NULL;
		}

		g_ptr_array_add(global_state->backend_pool, backend);
	}

	/* init the pool */
	for (i = 0; config->read_only_backend_addresses && 
			config->read_only_backend_addresses[i]; i++) {
		backend_t *backend;
		gchar *address = config->read_only_backend_addresses[i];

		backend = backend_init();
		backend->type = BACKEND_TYPE_RO;

		if (0 != network_mysqld_con_set_address(&backend->addr, address)) {
			return NULL;
		}

		g_ptr_array_add(global_state->backend_pool, backend);
	}

	return global_state;
}


NETWORK_MYSQLD_PLUGIN_PROTO(proxy_init) {
	plugin_con_state *st = con->plugin_con_state;
	chassis_private *priv = chas->priv;

	g_assert(con->plugin_con_state == NULL);

	st = plugin_con_state_init();

	if (NULL == (st->global_state = plugin_srv_state_get(con->config))) {
		return RET_ERROR;
	}

	con->plugin_con_state = st;
	
	con->state = CON_STATE_CONNECT_SERVER;

	return RET_SUCCESS;
}

static proxy_stmt_ret proxy_lua_disconnect_client(network_mysqld_con *con) {
	proxy_stmt_ret ret = PROXY_NO_DECISION;

#ifdef HAVE_LUA_H
	plugin_con_state *st = con->plugin_con_state;
	lua_State *L;

	/* call the lua script to pick a backend
	 * */
	lua_register_callback(con);

	if (!st->injected.L) return 0;

	L = st->injected.L;

	g_assert(lua_isfunction(L, -1));
	lua_getfenv(L, -1);
	g_assert(lua_istable(L, -1));
	
	lua_getfield_literal(L, -1, C("disconnect_client"));
	if (lua_isfunction(L, -1)) {
		if (lua_pcall(L, 0, 1, 0) != 0) {
			g_critical("%s.%d: (disconnect_client) %s", 
					__FILE__, __LINE__,
					lua_tostring(L, -1));

			lua_pop(L, 1); /* errmsg */

			/* the script failed, but we have a useful default */
		} else {
			if (lua_isnumber(L, -1)) {
				ret = lua_tonumber(L, -1);
			}
			lua_pop(L, 1);
		}

		switch (ret) {
		case PROXY_NO_DECISION:
		case PROXY_IGNORE_RESULT:
			break;
		default:
			ret = PROXY_NO_DECISION;
			break;
		}

		/* ret should be a index into */

	} else if (lua_isnil(L, -1)) {
		lua_pop(L, 1); /* pop the nil */
	} else {
		g_message("%s.%d: %s", __FILE__, __LINE__, lua_typename(L, lua_type(L, -1)));
		lua_pop(L, 1); /* pop the ... */
	}
	lua_pop(L, 1); /* fenv */

	g_assert(lua_isfunction(L, -1));
#endif
	return ret;
}


/**
 * cleanup the proxy specific data on the current connection 
 *
 * move the server connection into the connection pool in case it is a 
 * good client-side close
 *
 * @return RET_SUCCESS
 * @see plugin_call_cleanup
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_disconnect_client) {
	plugin_con_state *st = con->plugin_con_state;
	gboolean use_pooled_connection = FALSE;

	if (st == NULL) return RET_SUCCESS;
	
	/**
	 * let the lua-level decide if we want to keep the connection in the pool
	 */

	switch (proxy_lua_disconnect_client(con)) {
	case PROXY_NO_DECISION:
		/* just go on */

		break;
	case PROXY_IGNORE_RESULT:
		break;
	default:
		g_error("%s.%d: ... ", __FILE__, __LINE__);
		break;
	}

	/**
	 * check if one of the backends has to many open connections
	 */

	if (use_pooled_connection &&
	    con->state == CON_STATE_CLOSE_CLIENT) {
		/* move the connection to the connection pool
		 *
		 * this disconnects con->server and safes it from getting free()ed later
		 */

		proxy_connection_pool_add_connection(con);
	} else if (st->backend) {
		/* we have backend assigned and want to close the connection to it */
		st->backend->connected_clients--;
	}

	plugin_con_state_free(st);

	con->plugin_con_state = NULL;

	/**
	 * walk all pools and clean them up
	 */

	return RET_SUCCESS;
}

int network_mysqld_proxy_connection_init(network_mysqld_con *con) {
	con->plugins.con_init                      = proxy_init;
	con->plugins.con_connect_server            = proxy_connect_server;
	con->plugins.con_read_handshake            = proxy_read_handshake;
	con->plugins.con_read_auth                 = proxy_read_auth;
	con->plugins.con_read_auth_result          = proxy_read_auth_result;
	con->plugins.con_read_query                = proxy_read_query;
	con->plugins.con_read_query_result         = proxy_read_query_result;
	con->plugins.con_send_query_result         = proxy_send_query_result;
	con->plugins.con_cleanup                   = proxy_disconnect_client;

	return 0;
}

/**
 * free the global scope which is shared between all connections
 *
 * make sure that is called after all connections are closed
 */
void network_mysqld_proxy_free(network_mysqld_con *con) {
	plugin_srv_state *g = plugin_srv_state_get(NULL);

	plugin_srv_state_free(g);
}

chassis_plugin_config * network_mysqld_proxy_plugin_init(void) {
	chassis_plugin_config *config;

	config = g_new0(chassis_plugin_config, 1);
	config->fix_bug_25371   = 0; /** double ERR packet on AUTH failures */
	config->profiling       = 1;
	config->start_proxy     = 1;

	return config;
}

void network_mysqld_proxy_plugin_free(chassis_plugin_config *config) {
	gsize i;

	if (config->listen_con) {
		/**
		 * the connection will be free()ed by the network_mysqld_free()
		 */
#if 0
		event_del(&(config->listen_con->server->event));
		network_mysqld_con_free(config->listen_con);
#endif
	}

	if (config->backend_addresses) {
		for (i = 0; config->backend_addresses[i]; i++) {
			g_free(config->backend_addresses[i]);
		}
		g_free(config->backend_addresses);
	}

	if (config->address) {
		/* free the global scope */
		network_mysqld_proxy_free(NULL);

		g_free(config->address);
	}

	g_free(config);
}

/**
 * plugin options 
 */
static GOptionEntry * network_mysqld_proxy_plugin_get_options(chassis_plugin_config *config) {
	guint i;

	/* make sure it isn't collected */
	static GOptionEntry config_entries[] = 
	{
		{ "proxy-address",            0, 0, G_OPTION_ARG_STRING, NULL, "listening address:port of the proxy-server (default: :4040)", "<host:port>" },
		{ "proxy-read-only-backend-addresses", 
					      0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, "address:port of the remote slave-server (default: not set)", "<host:port>" },
		{ "proxy-backend-addresses",  0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, "address:port of the remote backend-servers (default: 127.0.0.1:3306)", "<host:port>" },
		
		{ "proxy-skip-profiling",     0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "disables profiling of queries (default: enabled)", NULL },

		{ "proxy-fix-bug-25371",      0, 0, G_OPTION_ARG_NONE, NULL, "fix bug #25371 (mysqld > 5.1.12) for older libmysql versions", NULL },
		{ "proxy-lua-script",         0, 0, G_OPTION_ARG_STRING, NULL, "filename of the lua script (default: not set)", "<file>" },
		
		{ "no-proxy",                 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "don't start the proxy-module (default: enabled)", NULL },
		
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	i = 0;
	config_entries[i++].arg_data = &(config->address);
	config_entries[i++].arg_data = &(config->read_only_backend_addresses);
	config_entries[i++].arg_data = &(config->backend_addresses);

	config_entries[i++].arg_data = &(config->profiling);

	config_entries[i++].arg_data = &(config->fix_bug_25371);
	config_entries[i++].arg_data = &(config->lua_script);
	config_entries[i++].arg_data = &(config->start_proxy);

	return config_entries;
}

/**
 * init the plugin with the parsed config
 */
int network_mysqld_proxy_plugin_apply_config(chassis *chas, chassis_plugin_config *config) {
	network_mysqld_con *con;
	network_socket *listen_sock;

	if (!config->start_proxy) {
		return 0;
	}

	if (!config->address) config->address = g_strdup(":4040");
	if (!config->backend_addresses) {
		config->backend_addresses = g_new0(char *, 2);
		config->backend_addresses[0] = g_strdup("127.0.0.1:3306");
	}

	/** 
	 * create a connection handle for the listen socket 
	 */
	con = network_mysqld_con_init();
	network_mysqld_add_connection(chas, con);
	con->config = config;

	config->listen_con = con;
	
	listen_sock = network_socket_init();
	con->server = listen_sock;

	/* set the plugin hooks as we want to apply them to the new connections too later */
	network_mysqld_proxy_connection_init(con);

	/* FIXME: network_socket_set_address() */
	if (0 != network_mysqld_con_set_address(&listen_sock->addr, config->address)) {
		return -1;
	}

	/* FIXME: network_socket_bind() */
	if (0 != network_mysqld_con_bind(listen_sock)) {
		return -1;
	}

	/**
	 * call network_mysqld_con_accept() with this connection when we are done
	 */

	event_set(&(listen_sock->event), listen_sock->fd, EV_READ|EV_PERSIST, network_mysqld_con_accept, con);
	event_base_set(chas->event_base, &(listen_sock->event));
	event_add(&(listen_sock->event), NULL);

	return 0;
}

int plugin_init(chassis_plugin *p) {
	p->magic        = CHASSIS_PLUGIN_MAGIC;
	p->name         = g_strdup("proxy");

	p->init         = network_mysqld_proxy_plugin_init;
	p->get_options  = network_mysqld_proxy_plugin_get_options;
	p->apply_config = network_mysqld_proxy_plugin_apply_config;
	p->destroy      = network_mysqld_proxy_plugin_free;

	return 0;
}

