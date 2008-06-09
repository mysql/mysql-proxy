/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifndef _NETWORK_MYSQLD_H_
#define _NETWORK_MYSQLD_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
/**
 * event.h needs struct timeval and doesn't include sys/time.h itself
 */
#include <sys/time.h>
#endif

#include <sys/types.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <winsock2.h>
#endif

#include <mysql.h>

#include <glib.h>

#include "network-exports.h"

#include "network-socket.h"
#include "network-conn-pool.h"
#include "chassis-plugin.h"
#include "chassis-mainloop.h"
#include "sys-pedantic.h"
#include "lua-scope.h"
#include "backend.h"


typedef struct network_mysqld_con network_mysqld_con; /* forward declaration */

/**
 * some plugins don't use the global "chas" pointer 
 */
#define NETWORK_MYSQLD_PLUGIN_FUNC(x) network_socket_retval_t (*x)(chassis *chas, network_mysqld_con *con)
#define NETWORK_MYSQLD_PLUGIN_PROTO(x) static network_socket_retval_t x(chassis G_GNUC_UNUSED *chas, network_mysqld_con *con)

typedef struct {
	NETWORK_MYSQLD_PLUGIN_FUNC(con_init);
	NETWORK_MYSQLD_PLUGIN_FUNC(con_connect_server);
	NETWORK_MYSQLD_PLUGIN_FUNC(con_read_handshake);
	NETWORK_MYSQLD_PLUGIN_FUNC(con_send_handshake);
	NETWORK_MYSQLD_PLUGIN_FUNC(con_read_auth);
	NETWORK_MYSQLD_PLUGIN_FUNC(con_send_auth);
	NETWORK_MYSQLD_PLUGIN_FUNC(con_read_auth_result);
	NETWORK_MYSQLD_PLUGIN_FUNC(con_send_auth_result);

	NETWORK_MYSQLD_PLUGIN_FUNC(con_read_query);
	NETWORK_MYSQLD_PLUGIN_FUNC(con_read_query_result);
	NETWORK_MYSQLD_PLUGIN_FUNC(con_send_query_result);
    
    NETWORK_MYSQLD_PLUGIN_FUNC(con_timer_elapsed);
    
	NETWORK_MYSQLD_PLUGIN_FUNC(con_cleanup);
} network_mysqld_hooks;

struct network_mysqld_con {
	/**
	 * SERVER:
	 * - CON_STATE_INIT
	 * - CON_STATE_SEND_HANDSHAKE
	 * - CON_STATE_READ_AUTH
	 * - CON_STATE_SEND_AUTH_RESULT
	 * - CON_STATE_READ_QUERY
	 * - CON_STATE_SEND_QUERY_RESULT
	 *
	 * Proxy does all states
	 *
	 * replication client needs some init to work
	 * - SHOW MASTER STATUS 
	 *   to get the binlog-file and the pos 
	 */
	enum { 
        CON_STATE_INIT, 
        CON_STATE_CONNECT_SERVER, 
        CON_STATE_READ_HANDSHAKE, 
        CON_STATE_SEND_HANDSHAKE, 
        CON_STATE_READ_AUTH, 
        CON_STATE_SEND_AUTH, 
        CON_STATE_READ_AUTH_RESULT,
        CON_STATE_SEND_AUTH_RESULT,
        CON_STATE_READ_AUTH_OLD_PASSWORD,
        CON_STATE_SEND_AUTH_OLD_PASSWORD,
        CON_STATE_READ_QUERY,
        CON_STATE_SEND_QUERY,
        CON_STATE_READ_QUERY_RESULT,
        CON_STATE_SEND_QUERY_RESULT,
        
        CON_STATE_CLOSE_CLIENT,
        CON_STATE_SEND_ERROR,
        CON_STATE_ERROR
    } state;                      /**< the current/next state of this connection */

	/**
	 * the client and server side of the connection
	 *
	 * each connection has a internal state
	 * - default_db
	 */
	network_socket *server, *client;

	int is_overlong_packet;

	network_mysqld_hooks plugins;
	chassis_plugin_config *config; /** config for this plugin */

	chassis *srv; /* our srv object */

	int is_listen_socket;

	gint8 auth_result_state;

	struct {
		guint32 len;
		enum enum_server_command command;

		gpointer data;
		void (*data_free)(gpointer);
	} parse;

	void *plugin_con_state;
};



NETWORK_API void g_list_string_free(gpointer data, gpointer UNUSED_PARAM(user_data));
NETWORK_API gboolean g_hash_table_true(gpointer UNUSED_PARAM(key), gpointer UNUSED_PARAM(value), gpointer UNUSED_PARAM(u));

NETWORK_API network_mysqld_con *network_mysqld_con_init(void);
NETWORK_API void network_mysqld_con_free(network_mysqld_con *con);

/** 
 * should be socket 
 */
NETWORK_API void network_mysqld_con_accept(int event_fd, short events, void *user_data); /** event handler for accept() */

NETWORK_API int network_mysqld_con_send_ok(network_socket *con);
NETWORK_API int network_mysqld_con_send_ok_full(network_socket *con, guint64 affected_rows, guint64 insert_id, guint16 server_status, guint16 warnings);
NETWORK_API int network_mysqld_con_send_error(network_socket *con, const gchar *errmsg, gsize errmsg_len);
NETWORK_API int network_mysqld_con_send_error_full(network_socket *con, const char *errmsg, gsize errmsg_len, guint errorcode, const gchar *sqlstate);
NETWORK_API int network_mysqld_con_send_resultset(network_socket *con, GPtrArray *fields, GPtrArray *rows);
NETWORK_API void network_mysqld_con_reset_command_response_state(network_mysqld_con *con);

/**
 * should be socket 
 */
NETWORK_API network_socket_retval_t network_mysqld_read(chassis *srv, network_socket *con);
NETWORK_API network_socket_retval_t network_mysqld_write(chassis *srv, network_socket *con);
NETWORK_API network_socket_retval_t network_mysqld_write_len(chassis *srv, network_socket *con, int send_chunks);

struct chassis_private {
	GPtrArray *cons;                          /**< array(network_mysqld_con) */

	lua_scope *sc;

	network_backends_t *backends;
};

NETWORK_API int network_mysqld_init(chassis *srv);
NETWORK_API void network_mysqld_add_connection(chassis *srv, network_mysqld_con *con);
NETWORK_API void network_mysqld_con_handle(int event_fd, short events, void *user_data);
NETWORK_API int network_mysqld_queue_append(network_queue *queue, const char *data, size_t len, int packet_id);

#endif
