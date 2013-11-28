/* $%BEGINLICENSE%$
 Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

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

/** 
 * @page page-plugin-proxy Proxy plugin
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <math.h> /* floor() */

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
#include "network-mysqld-packet.h"

#include "network-mysqld-lua.h"

#include "network-conn-pool.h"
#include "network-conn-pool-lua.h"

#include "sys-pedantic.h"
#include "network-injection.h"
#include "network-injection-lua.h"
#include "network-backend.h"
#include "glib-ext.h"
#include "lua-env.h"

#include "proxy-plugin.h"

#include "lua-load-factory.h"

#include "chassis-timings.h"
#include "chassis-gtimeval.h"

#define C(x) x, sizeof(x) - 1
#define S(x) x->str, x->len

/* backward compat with MySQL pre-5.5.7 */
#ifndef CLIENT_PLUGIN_AUTH
#define CLIENT_PLUGIN_AUTH (1 << 19)
#endif

#ifndef PLUGIN_VERSION
#ifdef CHASSIS_BUILD_TAG
#define PLUGIN_VERSION PACKAGE_VERSION "." CHASSIS_BUILD_TAG
#else
#define PLUGIN_VERSION PACKAGE_VERSION
#endif
#endif

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

struct chassis_plugin_config {
	gchar *address;                   /**< listening address of the proxy */

	gchar **backend_addresses;        /**< read-write backends */
	gchar **read_only_backend_addresses; /**< read-only  backends */

	gint fix_bug_25371;               /**< suppress the second ERR packet of bug #25371 */

	gint profiling;                   /**< skips the execution of the read_query() function */
	
	gchar *lua_script;                /**< script to load at the start the connection */

	gint pool_change_user;            /**< don't reset the connection, when a connection is taken from the pool
					       - this safes a round-trip, but we also don't cleanup the connection
					       - another name could be "fast-pool-connect", but that's too friendly
					       */

	gint start_proxy;

	network_mysqld_con *listen_con;

	gdouble connect_timeout_dbl; /* exposed in the config as double */
	gdouble read_timeout_dbl; /* exposed in the config as double */
	gdouble write_timeout_dbl; /* exposed in the config as double */
};

/**
 * handle event-timeouts on the different states
 *
 * @note con->state points to the current state
 *
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_timeout) {
	network_mysqld_con_lua_t *st = con->plugin_con_state;

	if (st == NULL) return NETWORK_SOCKET_ERROR;

	switch (con->state) {
	case CON_STATE_CONNECT_SERVER:
		if (con->server) {
			double timeout = con->connect_timeout.tv_sec +
				con->connect_timeout.tv_usec / 1000000.0;

			g_debug("%s: connecting to %s timed out after %.2f seconds. Trying another backend.",
					G_STRLOC,
					con->server->dst->name->str,
					timeout);

			st->backend->state = BACKEND_STATE_DOWN;
			chassis_gtime_testset_now(&st->backend->state_since, NULL);
			network_socket_free(con->server);
			con->server = NULL;

			/* stay in this state and let it pick another backend */

			return NETWORK_SOCKET_SUCCESS;
		}
		/* fall through */
	case CON_STATE_SEND_AUTH:
		if (con->server) {
			/* we tried to send the auth data to the server, but that timed out.
			 * send the client and error
			 */
			network_mysqld_con_send_error(con->client, C("backend timed out"));
			con->state = CON_STATE_SEND_AUTH_RESULT;
			return NETWORK_SOCKET_SUCCESS;
		}
		/* fall through */
	default:
		/* the client timed out, close the connection */
		con->state = CON_STATE_ERROR;
		return NETWORK_SOCKET_SUCCESS;
	}
}
	
static network_mysqld_lua_stmt_ret proxy_lua_read_query_result(network_mysqld_con *con) {
	network_socket *send_sock = con->client;
	network_socket *recv_sock = con->server;
	injection *inj = NULL;
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	network_mysqld_lua_stmt_ret ret = PROXY_NO_DECISION;

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
	switch(network_mysqld_con_lua_register_callback(con, con->config->lua_script)) {
		case REGISTER_CALLBACK_SUCCESS:
			break;
		case REGISTER_CALLBACK_LOAD_FAILED:
			network_mysqld_con_send_error(con->client, C("MySQL Proxy Lua script failed to load. Check the error log."));
			con->state = CON_STATE_SEND_ERROR;
			return PROXY_SEND_RESULT;
		case REGISTER_CALLBACK_EXECUTE_FAILED:
			network_mysqld_con_send_error(con->client, C("MySQL Proxy Lua script failed to execute. Check the error log."));
			con->state = CON_STATE_SEND_ERROR;
			return PROXY_SEND_RESULT;
	}
	

	if (st->L) {
		lua_State *L = st->L;

		g_assert(lua_isfunction(L, -1));
		lua_getfenv(L, -1);
		g_assert(lua_istable(L, -1));
		
		lua_getfield_literal(L, -1, C("read_query_result"));
		if (lua_isfunction(L, -1)) {
			injection **inj_p;
			GString *packet;

			inj_p = lua_newuserdata(L, sizeof(inj));
			*inj_p = inj;

			inj->result_queue = con->server->recv_queue->chunks;

			proxy_getinjectionmetatable(L);
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

			if (!con->resultset_is_needed && (PROXY_NO_DECISION != ret)) {
				/* if the user asks us to work on the resultset, but hasn't buffered it ... ignore the result */
				g_critical("%s: read_query_result() in %s tries to modify the resultset, but hasn't asked to buffer it in proxy.query:append(..., { resultset_is_needed = true }). We ignore the change to the result-set.", 
						G_STRLOC,
						con->config->lua_script);

				ret = PROXY_NO_DECISION;
			}

			switch (ret) {
			case PROXY_SEND_RESULT:
				g_assert_cmpint(con->resultset_is_needed, ==, TRUE); /* we can only replace the result, if we buffer it */
				/**
				 * replace the result-set the server sent us 
				 */
				while ((packet = g_queue_pop_head(recv_sock->recv_queue->chunks))) g_string_free(packet, TRUE);
				
				/**
				 * we are a response to the client packet, hence one packet id more 
				 */
				if (network_mysqld_con_lua_handle_proxy_response(con, con->config->lua_script)) {
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
					while ((packet = g_queue_pop_head(recv_sock->recv_queue->chunks))) {
						network_mysqld_queue_append_raw(send_sock, send_sock->send_queue, packet);
					}
					st->injected.sent_resultset++;
					break;
				}
				g_critical("%s.%d: got asked to send a resultset, but ignoring it as we already have sent %d resultset(s). injection-id: %d",
						__FILE__, __LINE__,
						st->injected.sent_resultset,
						inj->id);

				st->injected.sent_resultset++;

				/* fall through */
			case PROXY_IGNORE_RESULT:
				/* trash the packets for the injection query */

				if (!con->resultset_is_needed) {
					/* we can only ignore the result-set if we haven't forwarded it to the client already
					 *
					 * we can end up here if the lua script loops and sends more than one query and is 
					 * not buffering the resultsets. In that case we have to close the connection to
					 * the client as we get out of sync ... actually, if that happens it is already
					 * too late
					 * */

					g_critical("%s: we tried to send more than one resultset to the client, but didn't had them buffered. Now the client is out of sync may have closed the connection on us. Please use proxy.queries:append(..., { resultset_is_needed = true }); to fix this.", G_STRLOC);

					break;
				}

				while ((packet = g_queue_pop_head(recv_sock->recv_queue->chunks))) g_string_free(packet, TRUE);

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
static network_mysqld_lua_stmt_ret proxy_lua_read_handshake(network_mysqld_con *con) {
	network_mysqld_lua_stmt_ret ret = PROXY_NO_DECISION; /* send what the server gave us */
#ifdef HAVE_LUA_H
	network_mysqld_con_lua_t *st = con->plugin_con_state;

	lua_State *L;

	/* call the lua script to pick a backend
	   ignore the return code from network_mysqld_con_lua_register_callback, because we cannot do anything about it,
	   it would always show up as ERROR 2013, which is not helpful.
	 */
	(void)network_mysqld_con_lua_register_callback(con, con->config->lua_script);

	if (!st->L) return ret;

	L = st->L;

	g_assert(lua_isfunction(L, -1));
	lua_getfenv(L, -1);
	g_assert(lua_istable(L, -1));
	
	lua_getfield_literal(L, -1, C("read_handshake"));
	if (lua_isfunction(L, -1)) {
		/* export
		 *
		 * every thing we know about it
		 *  */

		if (lua_pcall(L, 0, 1, 0) != 0) {
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

			if (network_mysqld_con_lua_handle_proxy_response(con, con->config->lua_script)) {
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
	network_packet packet;
	network_socket *recv_sock, *send_sock;
	network_mysqld_auth_challenge *challenge;
	GString *challenge_packet;
	guint8 status = 0;
	int err = 0;

	send_sock = con->client;
	recv_sock = con->server;

 	packet.data = g_queue_peek_tail(recv_sock->recv_queue->chunks);
	packet.offset = 0;
	
	err = err || network_mysqld_proto_skip_network_header(&packet);
	if (err) return NETWORK_SOCKET_ERROR;

	err = err || network_mysqld_proto_peek_int8(&packet, &status);
	if (err) return NETWORK_SOCKET_ERROR;

	/* handle ERR packets directly */
	if (status == 0xff) {
		/* move the chunk from one queue to the next */
		network_mysqld_queue_append_raw(send_sock, send_sock->send_queue, g_queue_pop_tail(recv_sock->recv_queue->chunks));

		return NETWORK_SOCKET_ERROR; /* it sends what is in the send-queue and hangs up */
	}

	challenge = network_mysqld_auth_challenge_new();
	if (network_mysqld_proto_get_auth_challenge(&packet, challenge)) {
 		g_string_free(g_queue_pop_tail(recv_sock->recv_queue->chunks), TRUE);

		network_mysqld_auth_challenge_free(challenge);

		return NETWORK_SOCKET_ERROR;
	}

 	con->server->challenge = challenge;

	/* we can't sniff compressed packets nor do we support SSL */
	challenge->capabilities &= ~(CLIENT_COMPRESS);
	challenge->capabilities &= ~(CLIENT_SSL);

	switch (proxy_lua_read_handshake(con)) {
	case PROXY_NO_DECISION:
		break;
	case PROXY_SEND_RESULT:
		/* the client overwrote and wants to send its own packet
		 * it is already in the queue */

 		g_string_free(g_queue_pop_tail(recv_sock->recv_queue->chunks), TRUE);

		return NETWORK_SOCKET_ERROR;
	default:
		g_error("%s.%d: ...", __FILE__, __LINE__);
		break;
	}

	challenge_packet = g_string_sized_new(packet.data->len); /* the packet we generate will be likely as large as the old one. should save some reallocs */
	network_mysqld_proto_append_auth_challenge(challenge_packet, challenge);
	network_mysqld_queue_sync(send_sock, recv_sock);
	network_mysqld_queue_append(send_sock, send_sock->send_queue, S(challenge_packet));

	g_string_free(challenge_packet, TRUE);

	g_string_free(g_queue_pop_tail(recv_sock->recv_queue->chunks), TRUE);

	/* copy the pack to the client */
	g_assert(con->client->challenge == NULL);
	con->client->challenge = network_mysqld_auth_challenge_copy(challenge);
	
	con->state = CON_STATE_SEND_HANDSHAKE;

	return NETWORK_SOCKET_SUCCESS;
}

static network_mysqld_lua_stmt_ret proxy_lua_read_auth(network_mysqld_con *con) {
	network_mysqld_lua_stmt_ret ret = PROXY_NO_DECISION;

#ifdef HAVE_LUA_H
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	lua_State *L;

	/* call the lua script to pick a backend
	   ignore the return code from network_mysqld_con_lua_register_callback, because we cannot do anything about it,
	   it would always show up as ERROR 2013, which is not helpful.	
	*/
	(void)network_mysqld_con_lua_register_callback(con, con->config->lua_script);

	if (!st->L) return 0;

	L = st->L;

	g_assert(lua_isfunction(L, -1));
	lua_getfenv(L, -1);
	g_assert(lua_istable(L, -1));
	
	lua_getfield_literal(L, -1, C("read_auth"));
	if (lua_isfunction(L, -1)) {

		/* export
		 *
		 * every thing we know about it
		 *  */

		if (lua_pcall(L, 0, 1, 0) != 0) {
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

			if (network_mysqld_con_lua_handle_proxy_response(con, con->config->lua_script)) {
				/**
				 * handling proxy.response failed
				 *
				 * send a ERR packet
				 */
		
				network_mysqld_con_send_error(con->client, C("(lua) handling proxy.response failed, check error-log"));
			}

			break;
		case PROXY_SEND_QUERY:
			/* something is in the injection queue, pull it from there and replace the content of
			 * original packet */

			if (st->injected.queries->length) {
				ret = PROXY_SEND_INJECTION;
			} else {
				ret = PROXY_NO_DECISION;
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

NETWORK_MYSQLD_PLUGIN_PROTO(proxy_read_auth) {
	/* read auth from client */
	network_packet packet;
	network_socket *recv_sock, *send_sock;
	chassis_plugin_config *config = con->config;
	network_mysqld_auth_response *auth;
	int err = 0;
	gboolean free_client_packet = TRUE;
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	gboolean got_all_data = TRUE;

	recv_sock = con->client;
	send_sock = con->server;

 	packet.data = g_queue_peek_tail(recv_sock->recv_queue->chunks);
	packet.offset = 0;

	err = err || network_mysqld_proto_skip_network_header(&packet);
	if (err) return NETWORK_SOCKET_ERROR;

	/* assume that we may get called twice:
	 *
	 * 1. for the initial packet
	 * 2. for the win-auth extra data
	 *
	 * this is detected by con->client->response being NULL
	 */

	if (con->client->response == NULL) {
		auth = network_mysqld_auth_response_new(con->client->challenge->capabilities);

		err = err || network_mysqld_proto_get_auth_response(&packet, auth);

		if (err) {
			network_mysqld_auth_response_free(auth);
			return NETWORK_SOCKET_ERROR;
		}
		if (!(auth->client_capabilities & CLIENT_PROTOCOL_41)) {
			/* should use packet-id 0 */
			network_mysqld_queue_append(con->client, con->client->send_queue, C("\xff\xd7\x07" "4.0 protocol is not supported"));
			network_mysqld_auth_response_free(auth);
			return NETWORK_SOCKET_ERROR;
		}

 		con->client->response = auth;

		g_string_assign_len(con->client->default_db, S(auth->database));

		/* client and server support auth-plugins and the client uses
		 * win-auth, we may have more data to read from the client
		 */
		if ((auth->server_capabilities & CLIENT_PLUGIN_AUTH) &&
		    (auth->client_capabilities & CLIENT_PLUGIN_AUTH) &&
		    (strleq(S(auth->auth_plugin_name), C("authentication_windows_client"))) &&
		    (auth->auth_plugin_data->len == 255)) {
#if 1
			/**
			 * FIXME: the 2-packet win-auth protocol enhancements aren't properly tested yet.
			 * therefore they are disabled for now.
			 */
			g_string_free(g_queue_pop_head(con->client->recv_queue->chunks), TRUE);

			network_mysqld_con_send_error(con->client, C("long packets for windows-authentication aren't completely handled yet. Please use another auth-method for now."));

			return NETWORK_SOCKET_ERROR;
#else

			got_all_data = FALSE; /* strip the last byte as it is used for extra signaling that we should ignore */
			g_string_truncate(auth->auth_plugin_data, auth->auth_plugin_data->len - 1);
#endif
		} else {
			got_all_data = TRUE;
		}
	} else {
		GString *auth_data;
		gsize auth_data_len;

		/* this is the 2nd round. We don't expect more data */
		got_all_data = TRUE;

		/* get all the data from the packet and append it to the auth_plugin_data */
		auth_data_len = packet.data->len - 4;
		auth_data = g_string_sized_new(auth_data_len);
		network_mysqld_proto_get_gstring_len(&packet, auth_data_len, auth_data);

		g_string_append_len(con->client->response->auth_plugin_data, S(auth_data));

		g_string_free(auth_data, TRUE);
	}

	if (got_all_data) {
		/**
		 * looks like we finished parsing, call the lua function
		 */
		switch (proxy_lua_read_auth(con)) {
		case PROXY_SEND_RESULT:
			con->state = CON_STATE_SEND_AUTH_RESULT;

			break;
		case PROXY_SEND_INJECTION: {
			injection *inj;

			/* replace the client challenge that is sent to the server */
			inj = g_queue_pop_head(st->injected.queries);

			network_mysqld_queue_append(send_sock, send_sock->send_queue, S(inj->query));

			injection_free(inj);

			con->state = CON_STATE_SEND_AUTH;

			break; }
		case PROXY_NO_DECISION:
			/* if we don't have a backend (con->server), we just ack the client auth
			 */
			if (!con->server) {
				con->state = CON_STATE_SEND_AUTH_RESULT;

				network_mysqld_con_send_ok(recv_sock);

				break;
			}
			/* if the server-side of the connection is already up and authed
			 * we send a COM_CHANGE_USER to reauth the connection and remove
			 * all temp-tables and session-variables
			 *
			 * for performance reasons this extra reauth can be disabled. But
			 * that leaves temp-tables on the connection.
			 */
			if (con->server->is_authed) {
				if (config->pool_change_user) {
					GString *com_change_user = g_string_new(NULL);

					/* copy incl. the nul */
					g_string_append_c(com_change_user, COM_CHANGE_USER);
					g_string_append_len(com_change_user, con->client->response->username->str, con->client->response->username->len + 1); /* nul-term */

					g_assert_cmpint(con->client->response->auth_plugin_data->len, <, 250);

					g_string_append_c(com_change_user, (con->client->response->auth_plugin_data->len & 0xff));
					g_string_append_len(com_change_user, S(con->client->response->auth_plugin_data));

					g_string_append_len(com_change_user, con->client->default_db->str, con->client->default_db->len + 1);

					network_mysqld_proto_append_int16(com_change_user, con->client->response->charset);

					if (con->client->challenge->capabilities & CLIENT_PLUGIN_AUTH) {
						g_string_append_len(com_change_user, con->client->response->auth_plugin_name->str, con->client->response->auth_plugin_name->len + 1);
					}

					network_mysqld_queue_append(
							send_sock,
							send_sock->send_queue, 
							S(com_change_user));

					/* we just injected a com_change_user packet so let's set the flag to track it on the connection */
					st->is_in_com_change_user = TRUE;

					/**
					 * the server is already authenticated, the client isn't
					 *
					 * transform the auth-packet into a COM_CHANGE_USER
					 */

					g_string_free(com_change_user, TRUE);
				
					con->state = CON_STATE_SEND_AUTH;
				} else {
					GString *auth_resp;

					/* check if the username and client-scramble are the same as in the previous authed
					 * connection */

					auth_resp = g_string_new(NULL);

					con->state = CON_STATE_SEND_AUTH_RESULT;

					if (!g_string_equal(con->client->response->username, con->server->response->username) ||
					    !g_string_equal(con->client->response->auth_plugin_data, con->server->response->auth_plugin_data)) {
						network_mysqld_err_packet_t *err_packet;

						err_packet = network_mysqld_err_packet_new();
						g_string_assign_len(err_packet->errmsg, C("(proxy-pool) login failed"));
						g_string_assign_len(err_packet->sqlstate, C("28000"));
						err_packet->errcode = ER_ACCESS_DENIED_ERROR;

						network_mysqld_proto_append_err_packet(auth_resp, err_packet);

						network_mysqld_err_packet_free(err_packet);
					} else {
						network_mysqld_ok_packet_t *ok_packet;

						ok_packet = network_mysqld_ok_packet_new();
						ok_packet->server_status = SERVER_STATUS_AUTOCOMMIT;

						network_mysqld_proto_append_ok_packet(auth_resp, ok_packet);
						
						network_mysqld_ok_packet_free(ok_packet);
					}

					network_mysqld_queue_append(recv_sock, recv_sock->send_queue, 
							S(auth_resp));
					
					/* the server side of connection is already up and authed and we have checked that
					 * the username and client-scramble are the same as in the previous authed connection.
					 * the auth phase is over so we need to reset the packet-id sequence
					 */
					network_mysqld_queue_reset(send_sock);
					network_mysqld_queue_reset(recv_sock);

					g_string_free(auth_resp, TRUE);
				}
			} else {
				network_mysqld_queue_append_raw(send_sock, send_sock->send_queue, packet.data);
				con->state = CON_STATE_SEND_AUTH;

				free_client_packet = FALSE; /* the packet.data is now part of the send-queue, don't free it further down */
			}

			break;
		default:
			g_assert_not_reached();
			break;
		}

		if (free_client_packet) {
			g_string_free(g_queue_pop_tail(recv_sock->recv_queue->chunks), TRUE);
		} else {
			/* just remove the link to the packet, the packet itself is part of the next queue already */
			g_queue_pop_tail(recv_sock->recv_queue->chunks);
		}
	} else {
		/* move the packet from the recv-queue to the send-queue AS IS */

		network_mysqld_queue_append_raw(send_sock, send_sock->send_queue,
				g_queue_pop_tail(recv_sock->recv_queue->chunks));
		/* stay in this state and read the next packet */
	}

	return NETWORK_SOCKET_SUCCESS;
}

static network_mysqld_lua_stmt_ret proxy_lua_read_auth_result(network_mysqld_con *con) {
	network_mysqld_lua_stmt_ret ret = PROXY_NO_DECISION;

#ifdef HAVE_LUA_H
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	network_socket *recv_sock = con->server;
	GList *chunk = recv_sock->recv_queue->chunks->tail;
	GString *packet = chunk->data;
	lua_State *L;

	/* call the lua script to pick a backend
	   ignore the return code from network_mysqld_con_lua_register_callback, because we cannot do anything about it,
	   it would always show up as ERROR 2013, which is not helpful.	
	*/
	(void)network_mysqld_con_lua_register_callback(con, con->config->lua_script);

	if (!st->L) return 0;

	L = st->L;

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

			if (network_mysqld_con_lua_handle_proxy_response(con, con->config->lua_script)) {
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
	g_string_assign_len(recv_sock->default_db, S(send_sock->default_db));

	if (con->server->response) {
		/* in case we got the connection from the pool it has the response from the previous auth */
		network_mysqld_auth_response_free(con->server->response);
		con->server->response = NULL;
	}
	con->server->response = network_mysqld_auth_response_copy(con->client->response);

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
		network_mysqld_queue_append_raw(
				send_sock,
				send_sock->send_queue,
				packet);

		break;
	default:
		g_error("%s.%d: ... ", __FILE__, __LINE__);
		break;
	}

	/**
	 * we handled the packet on the server side, free it
	 */
	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);
	
	/* the auth phase is over
	 *
	 * reset the packet-id sequence
	 */
	network_mysqld_queue_reset(send_sock);
	network_mysqld_queue_reset(recv_sock);
	
	con->state = CON_STATE_SEND_AUTH_RESULT;

	return NETWORK_SOCKET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(proxy_read_auth_old_password) {
	network_socket *recv_sock, *send_sock;
	network_packet packet;
	guint32 packet_len;
	network_mysqld_con_lua_t *st = con->plugin_con_state;

	/* move the packet to the send queue */

	recv_sock = con->client;
	send_sock = con->server;

	if (NULL == con->server) {
		/**
		 * we have to auth against same backend as we did before
		 * but the user changed it
		 */

		network_mysqld_con_send_error(con->client, C("(lua) read-auth-old-password failed as backend_ndx got reset."));
		con->state = CON_STATE_SEND_ERROR;
		return NETWORK_SOCKET_SUCCESS;
	}

	packet.data = g_queue_peek_head(recv_sock->recv_queue->chunks);
	packet.offset = 0;

	packet_len = network_mysqld_proto_get_packet_len(packet.data);

	if ((strleq(S(con->auth_switch_to_method), C("authentication_windows_client"))) &&
	    (con->auth_switch_to_round == 0) &&
	    (packet_len == 255)) {
#if 1
		/**
		 * FIXME: the 2-packet win-auth protocol enhancements aren't properly tested yet.
		 * therefore they are disabled for now.
		 */
		g_string_free(g_queue_pop_head(recv_sock->recv_queue->chunks), TRUE);

		network_mysqld_con_send_error(recv_sock, C("long packets for windows-authentication aren't completely handled yet. Please use another auth-method for now."));

		con->state = CON_STATE_SEND_ERROR;
#else
		con->auth_switch_to_round++;
		/* move the packet to the send-queue
		 */
		network_mysqld_queue_append_raw(send_sock, send_sock->send_queue,
				g_queue_pop_head(recv_sock->recv_queue->chunks));

		/* stay in this state and read the next packet too */
#endif
	} else {
		/* let's check if the proxy plugin injected a com_change_user packet so we
		 * need to fix the packet-id
		 */
		if (st->is_in_com_change_user) {
			network_mysqld_proto_set_packet_id(packet.data, send_sock->last_packet_id + 1);
		}

		/* move the packet to the send-queue
		 */
		network_mysqld_queue_append_raw(send_sock, send_sock->send_queue,
				g_queue_pop_head(recv_sock->recv_queue->chunks));

		con->state = CON_STATE_SEND_AUTH_OLD_PASSWORD;
	}

	return NETWORK_SOCKET_SUCCESS;
}

static network_mysqld_lua_stmt_ret proxy_lua_read_query(network_mysqld_con *con) {
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	network_socket *recv_sock = con->client;
	GList   *chunk  = recv_sock->recv_queue->chunks->head;
	GString *packet = chunk->data;
	chassis_plugin_config *config = con->config;
	
	network_injection_queue_reset(st->injected.queries);

	/* ok, here we go */

#ifdef HAVE_LUA_H
	switch(network_mysqld_con_lua_register_callback(con, con->config->lua_script)) {
		case REGISTER_CALLBACK_SUCCESS:
			break;
		case REGISTER_CALLBACK_LOAD_FAILED:
			network_mysqld_con_send_error(con->client, C("MySQL Proxy Lua script failed to load. Check the error log."));
			con->state = CON_STATE_SEND_ERROR;
			return PROXY_SEND_RESULT;
		case REGISTER_CALLBACK_EXECUTE_FAILED:
			network_mysqld_con_send_error(con->client, C("MySQL Proxy Lua script failed to execute. Check the error log."));
			con->state = CON_STATE_SEND_ERROR;
			return PROXY_SEND_RESULT;
	}

	if (st->L) {
		lua_State *L = st->L;
		network_mysqld_lua_stmt_ret ret = PROXY_NO_DECISION;

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
			luaL_Buffer b;
			int i;

			/* pass the packet as parameter */
			luaL_buffinit(L, &b);
			/* iterate over the packets and append them all together */
			for (i = 0; NULL != (packet = g_queue_peek_nth(recv_sock->recv_queue->chunks, i)); i++) {
				luaL_addlstring(&b, packet->str + NET_HEADER_SIZE, packet->len - NET_HEADER_SIZE);
			}
			luaL_pushresult(&b);

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
	
				if (network_mysqld_con_lua_handle_proxy_response(con, con->config->lua_script)) {
					/**
					 * handling proxy.response failed
					 *
					 * send a ERR packet
					 */
			
					network_mysqld_con_send_error(con->client, C("(lua) handling proxy.response failed, check error-log"));
				}
	
				break;
			case PROXY_NO_DECISION:
				/* send on the data we got from the client unchanged
				 */

				if (st->injected.queries->length) {
					injection *inj;

					g_critical("%s: proxy.queue:append() or :prepend() used without 'return proxy.PROXY_SEND_QUERY'. Discarding %d elements from the queue.",
							G_STRLOC,
							st->injected.queries->length);

					while ((inj = g_queue_pop_head(st->injected.queries))) injection_free(inj);
				}
			
				break;
			case PROXY_SEND_QUERY:
				/* send the injected queries
				 *
				 * injection_new(..., query);
				 * 
				 *  */

				if (st->injected.queries->length == 0) {
					g_critical("%s: 'return proxy.PROXY_SEND_QUERY' used without proxy.queue:append() or :prepend(). Assuming 'nil' was returned",
							G_STRLOC);
				} else {
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
	network_socket *recv_sock, *send_sock;
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	int proxy_query = 1;
	network_mysqld_lua_stmt_ret ret;
	
	NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_query::enter");

	send_sock = NULL;
	recv_sock = con->client;
	st->injected.sent_resultset = 0;

	/* we already passed the CON_STATE_READ_AUTH_OLD_PASSWORD phase and sent all packets
	 * to the client so we need to set the COM_CHANGE_USER flag back to FALSE
	 */
	st->is_in_com_change_user = FALSE;

	NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_query::enter_lua");
	ret = proxy_lua_read_query(con);
	NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_query::leave_lua");

	/**
	 * if we disconnected in read_query_result() we have no connection open
	 * when we try to execute the next query 
	 *
	 * for PROXY_SEND_RESULT we don't need a server
	 */
	if (ret != PROXY_SEND_RESULT &&
	    con->server == NULL) {
		g_critical("%s.%d: I have no server backend, closing connection", __FILE__, __LINE__);
		return NETWORK_SOCKET_ERROR;
	}
	
	switch (ret) {
	case PROXY_NO_DECISION:
	case PROXY_SEND_QUERY:
		send_sock = con->server;

		/* no injection, pass on the chunks as is */
		while ((packet = g_queue_pop_head(recv_sock->recv_queue->chunks))) {
			network_mysqld_queue_append_raw(send_sock, send_sock->send_queue, packet);
		}
		con->resultset_is_needed = FALSE; /* we don't want to buffer the result-set */

		break;
	case PROXY_SEND_RESULT: {
		gboolean is_first_packet = TRUE;
		proxy_query = 0;

		send_sock = con->client;

		/* flush the recv-queue and track the command-states */
		while ((packet = g_queue_pop_head(recv_sock->recv_queue->chunks))) {
			if (is_first_packet) {
				network_packet p;

				p.data = packet;
				p.offset = 0;

				network_mysqld_con_reset_command_response_state(con);

				if (0 != network_mysqld_con_command_states_init(con, &p)) {
					g_debug("%s: ", G_STRLOC);
				}

				is_first_packet = FALSE;
			}

			g_string_free(packet, TRUE);
		}

		break; }
	case PROXY_SEND_INJECTION: {
		injection *inj;

		inj = g_queue_peek_head(st->injected.queries);
		con->resultset_is_needed = inj->resultset_is_needed; /* let the lua-layer decide if we want to buffer the result or not */

		send_sock = con->server;

		network_mysqld_queue_reset(send_sock);
		network_mysqld_queue_append(send_sock, send_sock->send_queue, S(inj->query));

		while ((packet = g_queue_pop_head(recv_sock->recv_queue->chunks))) g_string_free(packet, TRUE);

		break; }
	default:
		g_error("%s.%d: ", __FILE__, __LINE__);
	}

	if (proxy_query) {
		con->state = CON_STATE_SEND_QUERY;
	} else {
		GList *cur;

		/* if we don't send the query to the backend, it won't be tracked. So track it here instead 
		 * to get the packet tracking right (LOAD DATA LOCAL INFILE, ...) */

		for (cur = send_sock->send_queue->chunks->head; cur; cur = cur->next) {
			network_packet p;
			int r;

			p.data = cur->data;
			p.offset = 0;

			r = network_mysqld_proto_get_query_result(&p, con);
		}

		con->state = CON_STATE_SEND_QUERY_RESULT;
		con->resultset_is_finished = TRUE; /* we don't have more too send */
	}
	NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_query::done");

	return NETWORK_SOCKET_SUCCESS;
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
	network_mysqld_con_lua_t *st = con->plugin_con_state;

	send_sock = con->server;
	recv_sock = con->client;

	if (st->connection_close) {
		con->state = CON_STATE_ERROR;

		return NETWORK_SOCKET_SUCCESS;
	}

	if (con->parse.command == COM_BINLOG_DUMP) {
		/**
		 * the binlog dump is different as it doesn't have END packet
		 *
		 * @todo in 5.0.x a NON_BLOCKING option as added which sends a EOF
		 */
		con->state = CON_STATE_READ_QUERY_RESULT;

		return NETWORK_SOCKET_SUCCESS;
	}

	/* if we don't have a backend, don't try to forward queries
	 */
	if (!send_sock) {
		network_injection_queue_reset(st->injected.queries);
	}

	if (st->injected.queries->length == 0) {
		/* we have nothing more to send, let's see what the next state is */

		con->state = CON_STATE_READ_QUERY;

		return NETWORK_SOCKET_SUCCESS;
	}

	/* looks like we still have queries in the queue, 
	 * push the next one 
	 */
	inj = g_queue_peek_head(st->injected.queries);
	con->resultset_is_needed = inj->resultset_is_needed;

	if (!inj->resultset_is_needed && st->injected.sent_resultset > 0) {
		/* we already sent a resultset to the client and the next query wants to forward it's result-set too, that can't work */
		g_critical("%s: proxy.queries:append() in %s can only have one injected query without { resultset_is_needed = true } set. We close the client connection now.",
				G_STRLOC,
				con->config->lua_script);

		return NETWORK_SOCKET_ERROR;
	}

	g_assert(inj);
	g_assert(send_sock);

	network_mysqld_queue_reset(send_sock);
	network_mysqld_queue_append(send_sock, send_sock->send_queue, S(inj->query));

	network_mysqld_con_reset_command_response_state(con);

	con->state = CON_STATE_SEND_QUERY;

	return NETWORK_SOCKET_SUCCESS;
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
	network_packet packet;
	network_socket *recv_sock, *send_sock;
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	injection *inj = NULL;

	NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_query_result::enter");

	recv_sock = con->server;
	send_sock = con->client;

	/* check if the last packet is valid */
	packet.data = g_queue_peek_tail(recv_sock->recv_queue->chunks);
	packet.offset = 0;

	if (0 != st->injected.queries->length) {
		inj = g_queue_peek_head(st->injected.queries);
	}

	if (inj && inj->ts_read_query_result_first == 0) {
		/**
		 * log the time of the first received packet
		 */
		inj->ts_read_query_result_first = chassis_get_rel_microseconds();
		/* g_get_current_time(&(inj->ts_read_query_result_first)); */
	}

	is_finished = network_mysqld_proto_get_query_result(&packet, con);
	if (is_finished == -1) return NETWORK_SOCKET_ERROR; /* something happend, let's get out of here */

	con->resultset_is_finished = is_finished;

	/* copy the packet over to the send-queue if we don't need it */
	if (!con->resultset_is_needed) {
		network_mysqld_queue_append_raw(send_sock, send_sock->send_queue, g_queue_pop_tail(recv_sock->recv_queue->chunks));
	}

	if (is_finished) {
		network_mysqld_lua_stmt_ret ret;

		/**
		 * the resultset handler might decide to trash the send-queue
		 * 
		 * */

		if (inj) {
			if (con->parse.command == COM_QUERY || con->parse.command == COM_STMT_EXECUTE) {
				network_mysqld_com_query_result_t *com_query = con->parse.data;

				inj->bytes = com_query->bytes;
				inj->rows  = com_query->rows;
				inj->qstat.was_resultset = com_query->was_resultset;
				inj->qstat.binary_encoded = com_query->binary_encoded;

				/* INSERTs have a affected_rows */
				if (!com_query->was_resultset) {
					inj->qstat.affected_rows = com_query->affected_rows;
					inj->qstat.insert_id     = com_query->insert_id;
				}
				inj->qstat.server_status = com_query->server_status;
				inj->qstat.warning_count = com_query->warning_count;
				inj->qstat.query_status  = com_query->query_status;
			}
			inj->ts_read_query_result_last = chassis_get_rel_microseconds();
			/* g_get_current_time(&(inj->ts_read_query_result_last)); */
		}
		
		network_mysqld_queue_reset(recv_sock); /* reset the packet-id checks as the server-side is finished */

		NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_query_result::enter_lua");
		ret = proxy_lua_read_query_result(con);
		NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_query_result::leave_lua");

		if (PROXY_IGNORE_RESULT != ret) {
			/* reset the packet-id checks, if we sent something to the client */
			network_mysqld_queue_reset(send_sock);
		}

		/**
		 * if the send-queue is empty, we have nothing to send
		 * and can read the next query */
		if (send_sock->send_queue->chunks) {
			con->state = CON_STATE_SEND_QUERY_RESULT;
		} else {
			g_assert_cmpint(con->resultset_is_needed, ==, 1); /* we already forwarded the resultset, no way someone has flushed the resultset-queue */

			con->state = CON_STATE_READ_QUERY;
		}
	}
	NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_query_result::leave");
	
	return NETWORK_SOCKET_SUCCESS;
}

static network_mysqld_lua_stmt_ret proxy_lua_connect_server(network_mysqld_con *con) {
	network_mysqld_lua_stmt_ret ret = PROXY_NO_DECISION;

#ifdef HAVE_LUA_H
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	lua_State *L;

	/**
	 * if loading the script fails return a new error 
	 */
	switch (network_mysqld_con_lua_register_callback(con, con->config->lua_script)) {
	case REGISTER_CALLBACK_SUCCESS:
		break;
	case REGISTER_CALLBACK_LOAD_FAILED:
		/* send packet-id 0 */
		network_mysqld_con_send_error(con->client, C("MySQL Proxy Lua script failed to load. Check the error log."));
		return PROXY_SEND_RESULT;
	case REGISTER_CALLBACK_EXECUTE_FAILED:
		/* send packet-id 0 */
		network_mysqld_con_send_error(con->client, C("MySQL Proxy Lua script failed to execute. Check the error log."));
		return PROXY_SEND_RESULT;
	}

	if (!st->L) return PROXY_NO_DECISION;

	L = st->L;

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

			if (network_mysqld_con_lua_handle_proxy_response(con, con->config->lua_script)) {
				/**
				 * handling proxy.response failed
				 *
				 * send a ERR packet
				 */
		
				/* send packet-id 0 */
				network_mysqld_con_send_error(con->client, C("(lua) handling proxy.response failed, check error-log"));
			} else {
				network_queue *q;
				network_packet packet;
				int err = 0;
				guint8 packet_type;

				/* we should have a auth-packet or a err-packet in the queue */
				q = con->client->send_queue;

				packet.data = g_queue_peek_head(q->chunks);
				packet.offset = 0;

				err = err || network_mysqld_proto_skip_network_header(&packet);
				err = err || network_mysqld_proto_peek_int8(&packet, &packet_type);
				if (!err && packet_type == 0x0a) {
					network_mysqld_auth_challenge *challenge;

					challenge = network_mysqld_auth_challenge_new();

					err = err || network_mysqld_proto_get_auth_challenge(&packet, challenge);

					if (!err) {
						g_assert(con->client->challenge == NULL); /* make sure we don't leak memory */
						con->client->challenge = challenge;
					} else {
						network_mysqld_auth_challenge_free(challenge);
					}
				}
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
 *   NETWORK_SOCKET_SUCCESS        - connected successfully
 *   NETWORK_SOCKET_ERROR_RETRY    - connecting backend failed, call again to connect to another backend
 *   NETWORK_SOCKET_ERROR          - no backends available, adds a ERR packet to the client queue
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_connect_server) {
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	chassis_private *g = con->srv->priv;
	guint min_connected_clients = G_MAXUINT;
	guint i;
	gboolean use_pooled_connection = FALSE;
	network_backend_t *cur;

	if (con->server) {
		switch (network_socket_connect_finish(con->server)) {
		case NETWORK_SOCKET_SUCCESS:
			/* increment the connected clients value only if we connected successfully */
			st->backend->connected_clients++;
			break;
		case NETWORK_SOCKET_ERROR:
		case NETWORK_SOCKET_ERROR_RETRY:
			g_message("%s.%d: connect(%s) failed: %s. Retrying with different backend.", 
					__FILE__, __LINE__,
					con->server->dst->name->str, g_strerror(errno));

			/* mark the backend as being DOWN and retry with a different one */
			st->backend->state = BACKEND_STATE_DOWN;
			chassis_gtime_testset_now(&st->backend->state_since, NULL);
			network_socket_free(con->server);
			con->server = NULL;

			return NETWORK_SOCKET_ERROR_RETRY;
		default:
			g_assert_not_reached();
			break;
		}

		if (st->backend->state != BACKEND_STATE_UP) {
			st->backend->state = BACKEND_STATE_UP;
			chassis_gtime_testset_now(&st->backend->state_since, NULL);
		}

		con->state = CON_STATE_READ_HANDSHAKE;

		return NETWORK_SOCKET_SUCCESS;
	}

	st->backend = NULL;
	st->backend_ndx = -1;

	network_backends_check(g->backends);

	switch (proxy_lua_connect_server(con)) {
	case PROXY_SEND_RESULT:
		/* we answered directly ... like denial ...
		 *
		 * for sure we have something in the send-queue 
		 *
		 */
		
		return NETWORK_SOCKET_SUCCESS;
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

	/* protect the typecast below */
	g_assert_cmpint(g->backends->backends->len, <, G_MAXINT);

	/**
	 * if the current backend is down, ignore it 
	 */
	cur = network_backends_get(g->backends, st->backend_ndx);

	if (cur) {
		if (cur->state == BACKEND_STATE_DOWN) {
			st->backend_ndx = -1;
		}
	}

	if (con->server && !use_pooled_connection) {
		gint bndx = st->backend_ndx;
		/* we already have a connection assigned, 
		 * but the script said we don't want to use it
		 */

		network_connection_pool_lua_add_connection(con);

		st->backend_ndx = bndx;
	}

	if (st->backend_ndx < 0) {
		/**
		 * we can choose between different back addresses 
		 *
		 * prefer SQF (shorted queue first) to load all backends equally
		 */ 

		for (i = 0; i < network_backends_count(g->backends); i++) {
			cur = network_backends_get(g->backends, i);
	
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

		if ((cur = network_backends_get(g->backends, st->backend_ndx))) {
			st->backend = cur;
		}
	} else if (NULL == st->backend) {
		if ((cur = network_backends_get(g->backends, st->backend_ndx))) {
			st->backend = cur;
		}
	}

	if (NULL == st->backend) {
		network_mysqld_con_send_error_pre41(con->client, C("(proxy) all backends are down"));
		g_critical("%s.%d: Cannot connect, all backends are down.", __FILE__, __LINE__);
		return NETWORK_SOCKET_ERROR;
	}

	/**
	 * check if we have a connection in the pool for this backend
	 */
	if (NULL == con->server) {
		con->server = network_socket_new();
		network_address_copy(con->server->dst, st->backend->addr);

		switch(network_socket_connect(con->server)) {
		case NETWORK_SOCKET_ERROR_RETRY:
			/* the socket is non-blocking already, 
			 * call getsockopt() to see if we are done */
			return NETWORK_SOCKET_ERROR_RETRY;
		case NETWORK_SOCKET_SUCCESS:
			/* increment the connected clients value only if we connected successfully */
			st->backend->connected_clients++;
			break;
		default:
			g_message("%s.%d: connecting to backend (%s) failed, marking it as down for ...", 
					__FILE__, __LINE__, con->server->dst->name->str);

			st->backend->state = BACKEND_STATE_DOWN;
			chassis_gtime_testset_now(&st->backend->state_since, NULL);

			network_socket_free(con->server);
			con->server = NULL;

			return NETWORK_SOCKET_ERROR_RETRY;
		}

		if (st->backend->state != BACKEND_STATE_UP) {
			st->backend->state = BACKEND_STATE_UP;
			chassis_gtime_testset_now(&st->backend->state_since, NULL);
		}

		con->state = CON_STATE_READ_HANDSHAKE;
	} else {
		GString *auth_packet;

		/**
		 * send the old hand-shake packet
		 */

		auth_packet = g_string_new(NULL);
		network_mysqld_proto_append_auth_challenge(auth_packet, con->server->challenge);

		network_mysqld_queue_append(
				con->client,
				con->client->send_queue, 
				S(auth_packet));

		g_string_free(auth_packet, TRUE);

		g_assert(con->client->challenge == NULL);
		con->client->challenge = network_mysqld_auth_challenge_copy(con->server->challenge);

		con->state = CON_STATE_SEND_HANDSHAKE;

		/**
		 * connect_clients is already incremented 
		 */
	}

	return NETWORK_SOCKET_SUCCESS;
}

/**
 * convert a double into a timeval
 */
static gboolean
timeval_from_double(struct timeval *dst, double t) {
	g_return_val_if_fail(dst != NULL, FALSE);
	g_return_val_if_fail(t >= 0, FALSE);

	dst->tv_sec = floor(t);
	dst->tv_usec = floor((t - dst->tv_sec) * 1000000);

	return TRUE;
}

NETWORK_MYSQLD_PLUGIN_PROTO(proxy_init) {
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	chassis_plugin_config *config = con->config;

	g_assert(con->plugin_con_state == NULL);

	st = network_mysqld_con_lua_new();

	con->plugin_con_state = st;
	
	con->state = CON_STATE_CONNECT_SERVER;

	/* set the connection specific timeouts
	 *
	 * TODO: expose these settings at runtime
	 */
	if (config->connect_timeout_dbl >= 0) {
		timeval_from_double(&con->connect_timeout, config->connect_timeout_dbl);
	}
	if (config->read_timeout_dbl >= 0) {
		timeval_from_double(&con->read_timeout, config->read_timeout_dbl);
	}
	if (config->write_timeout_dbl >= 0) {
		timeval_from_double(&con->write_timeout, config->write_timeout_dbl);
	}



	return NETWORK_SOCKET_SUCCESS;
}

static network_mysqld_lua_stmt_ret proxy_lua_disconnect_client(network_mysqld_con *con) {
	network_mysqld_lua_stmt_ret ret = PROXY_NO_DECISION;

#ifdef HAVE_LUA_H
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	lua_State *L;

	/* call the lua script to pick a backend
	 * */
	/* this error handling is different, as we no longer have a client. */
	switch(network_mysqld_con_lua_register_callback(con, con->config->lua_script)) {
		case REGISTER_CALLBACK_SUCCESS:
			break;
		case REGISTER_CALLBACK_LOAD_FAILED:
		case REGISTER_CALLBACK_EXECUTE_FAILED:
			return ret;
	}

	if (!st->L) return 0;

	L = st->L;

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
 * @return NETWORK_SOCKET_SUCCESS
 * @see plugin_call_cleanup
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_disconnect_client) {
	network_mysqld_con_lua_t *st = con->plugin_con_state;
	lua_scope  *sc = con->srv->priv->sc;
	gboolean use_pooled_connection = FALSE;

	if (st == NULL) return NETWORK_SOCKET_SUCCESS;
	
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

		network_connection_pool_lua_add_connection(con);
	} else if (st->backend) {
		/* we have backend assigned and want to close the connection to it */
		st->backend->connected_clients--;
	}

#ifdef HAVE_LUA_H
	/* remove this cached script from registry */
	if (st->L_ref > 0) {
		luaL_unref(sc->L, LUA_REGISTRYINDEX, st->L_ref);
	}
#endif

	network_mysqld_con_lua_free(st);

	con->plugin_con_state = NULL;

	/**
	 * walk all pools and clean them up
	 */

	return NETWORK_SOCKET_SUCCESS;
}

/**
 * read the load data infile data from the client
 *
 * - decode the result-set to track if we are finished already
 * - gets called once for each packet
 *
 * @FIXME stream the data to the backend
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_read_local_infile_data) {
	int query_result = 0;
	network_packet packet;
	network_socket *recv_sock, *send_sock;
	network_mysqld_com_query_result_t *com_query = con->parse.data;

	NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_query_result::enter");
	
	recv_sock = con->client;
	send_sock = con->server;

	/* check if the last packet is valid */
	packet.data = g_queue_peek_tail(recv_sock->recv_queue->chunks);
	packet.offset = 0;

	/* if we get here from another state, src/network-mysqld.c is broken */
	g_assert_cmpint(con->parse.command, ==, COM_QUERY);
	g_assert_cmpint(com_query->state, ==, PARSE_COM_QUERY_LOCAL_INFILE_DATA);

	query_result = network_mysqld_proto_get_query_result(&packet, con);

	/* set the testing flag for all data received or not */ 
	con->local_file_data_is_finished = (query_result == 1);

	if (query_result == -1) return NETWORK_SOCKET_ERROR; /* something happend, let's get out of here */

	if (con->server) {
		/* we haven't received all data from load data infile, so let's continue reading and writing to the backend */
		network_mysqld_queue_append_raw(send_sock, send_sock->send_queue,
				g_queue_pop_tail(recv_sock->recv_queue->chunks));
	} else {
		GString *s;
		/* we don't have a backend
		 *
		 * - free the received packets early
		 * - send a OK later 
		 */
		while ((s = g_queue_pop_head(recv_sock->recv_queue->chunks))) g_string_free(s, TRUE);
	}

	if (query_result == 1) {
		if (con->server) { /* we have received all data, lets move forward reading the result from the server */
			con->state = CON_STATE_SEND_LOCAL_INFILE_DATA;
		} else {
			network_mysqld_con_send_ok(con->client);
			con->state = CON_STATE_SEND_LOCAL_INFILE_RESULT;
		}
		g_assert_cmpint(com_query->state, ==, PARSE_COM_QUERY_LOCAL_INFILE_RESULT);
	}

	return NETWORK_SOCKET_SUCCESS;
}

/**
 * read the load data infile result from the server
 *
 * - decode the result-set to track if we are finished already
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_read_local_infile_result) {
	int query_result = 0;
	network_packet packet;
	network_socket *recv_sock, *send_sock;

	NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::ready_local_infile_result::enter");

	recv_sock = con->server;
	send_sock = con->client;

	/* check if the last packet is valid */
	packet.data = g_queue_peek_tail(recv_sock->recv_queue->chunks);
	packet.offset = 0;
	
	query_result = network_mysqld_proto_get_query_result(&packet, con);
	if (query_result == -1) return NETWORK_SOCKET_ERROR; /* something happend, let's get out of here */

	network_mysqld_queue_append_raw(send_sock, send_sock->send_queue,
			g_queue_pop_tail(recv_sock->recv_queue->chunks));

	if (query_result == 1) {
		con->state = CON_STATE_SEND_LOCAL_INFILE_RESULT;
	}

	return NETWORK_SOCKET_SUCCESS;
}

/**
 * cleanup after we sent to result of the LOAD DATA INFILE LOCAL data to the client
 */
NETWORK_MYSQLD_PLUGIN_PROTO(proxy_send_local_infile_result) {
	network_socket *recv_sock, *send_sock;

	NETWORK_MYSQLD_CON_TRACK_TIME(con, "proxy::send_local_infile_result::enter");

	recv_sock = con->server;
	send_sock = con->client;

	/* reset the packet-ids */
	if (send_sock) network_mysqld_queue_reset(send_sock);
	if (recv_sock) network_mysqld_queue_reset(recv_sock);

	con->state = CON_STATE_READ_QUERY;

	return NETWORK_SOCKET_SUCCESS;
}


int network_mysqld_proxy_connection_init(network_mysqld_con *con) {
	con->plugins.con_init                      = proxy_init;
	con->plugins.con_connect_server            = proxy_connect_server;
	con->plugins.con_read_handshake            = proxy_read_handshake;
	con->plugins.con_read_auth                 = proxy_read_auth;
	con->plugins.con_read_auth_result          = proxy_read_auth_result;
	con->plugins.con_read_auth_old_password	   = proxy_read_auth_old_password;
	con->plugins.con_read_query                = proxy_read_query;
	con->plugins.con_read_query_result         = proxy_read_query_result;
	con->plugins.con_send_query_result         = proxy_send_query_result;
	con->plugins.con_read_local_infile_data = proxy_read_local_infile_data;
	con->plugins.con_read_local_infile_result = proxy_read_local_infile_result;
	con->plugins.con_send_local_infile_result = proxy_send_local_infile_result;
	con->plugins.con_cleanup                   = proxy_disconnect_client;
	con->plugins.con_timeout                   = proxy_timeout;

	return 0;
}

/**
 * free the global scope which is shared between all connections
 *
 * make sure that is called after all connections are closed
 */
void network_mysqld_proxy_free(network_mysqld_con G_GNUC_UNUSED *con) {
}

chassis_plugin_config * network_mysqld_proxy_plugin_new(void) {
	chassis_plugin_config *config;

	config = g_new0(chassis_plugin_config, 1);
	config->fix_bug_25371   = 0; /** double ERR packet on AUTH failures */
	config->profiling       = 1;
	config->start_proxy     = 1;
	config->pool_change_user = 1; /* issue a COM_CHANGE_USER to cleanup the connection 
					 when we get back the connection from the pool */

	/* use negative values as defaults to make them ignored */
	config->connect_timeout_dbl = -1.0;
	config->read_timeout_dbl = -1.0;
	config->write_timeout_dbl = -1.0;

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

	if (config->lua_script) g_free(config->lua_script);

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
		{ "proxy-address",            'P', 0, G_OPTION_ARG_STRING, NULL, "listening address:port of the proxy-server (default: :4040)", "<host:port>" },
		{ "proxy-read-only-backend-addresses", 
					      'r', 0, G_OPTION_ARG_STRING_ARRAY, NULL, "address:port of the remote slave-server (default: not set)", "<host:port>" },
		{ "proxy-backend-addresses",  'b', 0, G_OPTION_ARG_STRING_ARRAY, NULL, "address:port of the remote backend-servers (default: 127.0.0.1:3306)", "<host:port>" },
		
		{ "proxy-skip-profiling",     0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "disables profiling of queries (default: enabled)", NULL },

		{ "proxy-fix-bug-25371",      0, 0, G_OPTION_ARG_NONE, NULL, "fix bug #25371 (mysqld > 5.1.12) for older libmysql versions", NULL },
		{ "proxy-lua-script",         's', 0, G_OPTION_ARG_FILENAME, NULL, "filename of the lua script (default: not set)", "<file>" },
		
		{ "no-proxy",                 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "don't start the proxy-module (default: enabled)", NULL },
		
		{ "proxy-pool-no-change-user", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "don't use CHANGE_USER to reset the connection coming from the pool (default: enabled)", NULL },

		{ "proxy-connect-timeout",    0, 0, G_OPTION_ARG_DOUBLE, NULL, "connect timeout in seconds (default: 2.0 seconds)", NULL },
		{ "proxy-read-timeout",    0, 0, G_OPTION_ARG_DOUBLE, NULL, "read timeout in seconds (default: 8 hours)", NULL },
		{ "proxy-write-timeout",    0, 0, G_OPTION_ARG_DOUBLE, NULL, "write timeout in seconds (default: 8 hours)", NULL },
		
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
	config_entries[i++].arg_data = &(config->pool_change_user);
	config_entries[i++].arg_data = &(config->connect_timeout_dbl);
	config_entries[i++].arg_data = &(config->read_timeout_dbl);
	config_entries[i++].arg_data = &(config->write_timeout_dbl);

	return config_entries;
}

/**
 * init the plugin with the parsed config
 */
int network_mysqld_proxy_plugin_apply_config(chassis *chas, chassis_plugin_config *config) {
	network_mysqld_con *con;
	network_socket *listen_sock;
	chassis_private *g = chas->priv;
	guint i;

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
	con = network_mysqld_con_new();
	network_mysqld_add_connection(chas, con);
	con->config = config;

	config->listen_con = con;
	
	listen_sock = network_socket_new();
	con->server = listen_sock;

	/* set the plugin hooks as we want to apply them to the new connections too later */
	network_mysqld_proxy_connection_init(con);

	if (0 != network_address_set_address(listen_sock->dst, config->address)) {
		return -1;
	}

	if (0 != network_socket_bind(listen_sock)) {
		return -1;
	}
	g_message("proxy listening on port %s", config->address);

	for (i = 0; config->backend_addresses && config->backend_addresses[i]; i++) {
		if (-1 == network_backends_add(g->backends, config->backend_addresses[i],
				BACKEND_TYPE_RW)) {
			return -1;
		}
	}
	
	for (i = 0; config->read_only_backend_addresses && config->read_only_backend_addresses[i]; i++) {
		if (-1 == network_backends_add(g->backends,
				config->read_only_backend_addresses[i], BACKEND_TYPE_RO)) {
			return -1;
		}
	}

	/* load the script and setup the global tables */
	network_mysqld_lua_setup_global(chas->priv->sc->L, g);

	/**
	 * call network_mysqld_con_accept() with this connection when we are done
	 */

	event_set(&(listen_sock->event), listen_sock->fd, EV_READ|EV_PERSIST, network_mysqld_con_accept, con);
	event_base_set(chas->event_base, &(listen_sock->event));
	event_add(&(listen_sock->event), NULL);

	return 0;
}

G_MODULE_EXPORT int plugin_init(chassis_plugin *p) {
	p->magic        = CHASSIS_PLUGIN_MAGIC;
	p->name         = g_strdup("proxy");
	p->version		= g_strdup(PLUGIN_VERSION);

	p->init         = network_mysqld_proxy_plugin_new;
	p->get_options  = network_mysqld_proxy_plugin_get_options;
	p->apply_config = network_mysqld_proxy_plugin_apply_config;
	p->destroy      = network_mysqld_proxy_plugin_free;

	return 0;
}

