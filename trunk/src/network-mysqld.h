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

/**
 * stolen from sql/log_event.h
 *
 * (MySQL 5.1.12)
 */
enum Log_event_type
{
  /*
    Every time you update this enum (when you add a type), you have to
    fix Format_description_log_event::Format_description_log_event().
  */
  UNKNOWN_EVENT= 0,
  START_EVENT_V3= 1,
  QUERY_EVENT= 2,
  STOP_EVENT= 3,
  ROTATE_EVENT= 4,
  INTVAR_EVENT= 5,
  LOAD_EVENT= 6,
  SLAVE_EVENT= 7,
  CREATE_FILE_EVENT= 8,
  APPEND_BLOCK_EVENT= 9,
  EXEC_LOAD_EVENT= 10,
  DELETE_FILE_EVENT= 11,
  /*
    NEW_LOAD_EVENT is like LOAD_EVENT except that it has a longer
    sql_ex, allowing multibyte TERMINATED BY etc; both types share the
    same class (Load_log_event)
  */
  NEW_LOAD_EVENT= 12,
  RAND_EVENT= 13,
  USER_VAR_EVENT= 14,
  FORMAT_DESCRIPTION_EVENT= 15,
  XID_EVENT= 16,
  BEGIN_LOAD_QUERY_EVENT= 17,
  EXECUTE_LOAD_QUERY_EVENT= 18,
  TABLE_MAP_EVENT = 19,
  WRITE_ROWS_EVENT = 20,
  UPDATE_ROWS_EVENT = 21,
  DELETE_ROWS_EVENT = 22,

  /*
    Add new events here - right above this comment!
    Existing events (except ENUM_END_EVENT) should never change their numbers
  */

  ENUM_END_EVENT /* end marker */
};


typedef enum {
	RET_SUCCESS,
	RET_WAIT_FOR_EVENT,
	RET_ERROR,
	RET_ERROR_RETRY
} retval_t;

typedef struct network_mysqld_con network_mysqld_con; /* forward declaration */

/**
 * some plugins don't use the global "chas" pointer 
 */
#define NETWORK_MYSQLD_PLUGIN_FUNC(x) retval_t (*x)(chassis *chas, network_mysqld_con *con)
#define NETWORK_MYSQLD_PLUGIN_PROTO(x) static retval_t x(chassis G_GNUC_UNUSED *chas, network_mysqld_con *con)

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

	struct {
		guint32 len;
		enum enum_server_command command;

		/**
		 * each state can add their local parsing information
		 *
		 * auth_result is used to track the state
		 * - OK  is fine
		 * - ERR will close the connection
		 * - EOF asks for old-password
		 */
		union {
			struct {
				int want_eofs;
				int first_packet;
			} prepare;

			enum {
				PARSE_COM_QUERY_INIT,
				PARSE_COM_QUERY_FIELD,
				PARSE_COM_QUERY_RESULT,
				PARSE_COM_QUERY_LOAD_DATA,
				PARSE_COM_QUERY_LOAD_DATA_END_DATA
			} query;

			struct {
				char state; /** OK, EOF, ERR */
			} auth_result;

			/** track the db in the COM_INIT_DB */
			struct {
				GString *db_name;
			} init_db;
		} state;
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
NETWORK_API int network_mysqld_con_set_address(network_address *addr, gchar *address);
NETWORK_API int network_mysqld_con_connect(network_socket *con);
NETWORK_API int network_mysqld_con_bind(network_socket *con);
NETWORK_API void network_mysqld_con_accept(int event_fd, short events, void *user_data); /** event handler for accept() */

NETWORK_API int network_queue_append(network_queue *queue, const char *data, size_t len, int packet_id);
NETWORK_API int network_queue_append_chunk(network_queue *queue, GString *chunk);

NETWORK_API int network_mysqld_con_send_ok(network_socket *con);
NETWORK_API int network_mysqld_con_send_ok_full(network_socket *con, guint64 affected_rows, guint64 insert_id, guint16 server_status, guint16 warnings);
NETWORK_API int network_mysqld_con_send_error(network_socket *con, const gchar *errmsg, gsize errmsg_len);
NETWORK_API int network_mysqld_con_send_error_full(network_socket *con, const char *errmsg, gsize errmsg_len, guint errorcode, const gchar *sqlstate);
NETWORK_API int network_mysqld_con_send_resultset(network_socket *con, GPtrArray *fields, GPtrArray *rows);

/**
 * should be socket 
 */
NETWORK_API retval_t network_mysqld_read(chassis *srv, network_socket *con);
NETWORK_API retval_t network_mysqld_write(chassis *srv, network_socket *con);
NETWORK_API retval_t network_mysqld_write_len(chassis *srv, network_socket *con, int send_chunks);

struct chassis_private {
	GPtrArray *cons;                          /**< array(network_mysqld_con) */

	lua_scope *sc;
};

NETWORK_API int network_mysqld_init(chassis *srv);
NETWORK_API void network_mysqld_add_connection(chassis *srv, network_mysqld_con *con);
NETWORK_API void network_mysqld_con_handle(int event_fd, short events, void *user_data);

/**
 * socket handling 
 */
NETWORK_API network_socket *network_socket_init(void);

#endif
