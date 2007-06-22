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
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <unistd.h>

#include <mysql.h>

#include <glib.h>

#include <event.h>

#include "sys-pedantic.h"

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

#define MYSQLD_PACKET_OK   (0)
#define MYSQLD_PACKET_NULL (-5) /* 0xfb */
                                /* 0xfc */
                                /* 0xfd */
#define MYSQLD_PACKET_EOF  (-2) /* 0xfe */
#define MYSQLD_PACKET_ERR  (-1) /* 0xff */

#define PACKET_LEN_UNSET   (0xffffffff)
#define PACKET_LEN_MAX     (0x00ffffff)


typedef enum {
	RET_SUCCESS,
	RET_WAIT_FOR_EVENT,
	RET_ERROR,
	RET_ERROR_RETRY
} retval_t;

/**
 * TODO: move sub-structs into the plugins 
 */
typedef struct {
	struct {
		gchar *address;
	} admin;

	struct {
		gchar *address;
		gchar *read_only_address;         /** all connections coming in on this port are read-only (to the slaves) */

		gchar **backend_addresses;        /** write master backends */

		gint fix_bug_25371;

		gint profiling;
		
		gchar *lua_script;
	} proxy;

	enum { NETWORK_TYPE_SERVER, NETWORK_TYPE_PROXY } network_type;
} network_mysqld_config;

typedef struct {
	struct event_base *event_base;

	network_mysqld_config config;

	GHashTable *index_usage;
	struct {
		guint64 queries;
	} stats;

	GPtrArray *cons;
	GHashTable *tables;
} network_mysqld;

/* a input or output stream */
typedef struct {
	GQueue *chunks;

	size_t len; /* len in all chunks */
	size_t offset; /* offset over all chunks */
} network_queue;

typedef struct {
	union {
		struct sockaddr_in ipv4;
	} addr;

	gchar *str;

	socklen_t len;
} network_address;

typedef struct {
	int fd;
	struct event event;

	network_address addr;

	guint32 packet_len; /* the packet_len is a 24bit unsigned int */

	int packet_id;
	int thread_id;

	network_queue *recv_queue;
	network_queue *recv_raw_queue;
	network_queue *send_queue;

	unsigned char header[4];
	off_t header_read;
	off_t to_read;
	
	int mysqld_version;
} network_socket;

typedef struct network_mysqld_con network_mysqld_con; /* forward declaration */

#define NETWORK_MYSQLD_PLUGIN_FUNC(x) retval_t (*x)(network_mysqld *srv, network_mysqld_con *con)
#define NETWORK_MYSQLD_PLUGIN_PROTO(x) static retval_t x(network_mysqld *srv, network_mysqld_con *con)

typedef struct network_mysqld_plugins {
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
	NETWORK_MYSQLD_PLUGIN_FUNC(con_cleanup);
} network_mysqld_plugins;

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
		CON_STATE_READ_QUERY,
		CON_STATE_SEND_QUERY,
		CON_STATE_READ_QUERY_RESULT,
		CON_STATE_SEND_QUERY_RESULT,

		CON_STATE_ERROR
       	} state;

	network_socket *server, *client;

	int is_overlong_packet;

	network_mysqld_plugins plugins;
	network_mysqld_config config;
	network_mysqld *srv; /* our srv object */

	int is_listen_socket;
	GString *default_db;

	char *filename;                /** file-name of the log-file */

	struct {
		guint32 len;
		enum enum_server_command command;

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
		} state;
	} parse;

	void *plugin_con_state;
};

typedef struct {
	guint max_used_key_len;
	double avg_used_key_len;

	guint64 used;
} network_mysqld_index_status;

typedef struct {
	int (*select)(GPtrArray *fields, GPtrArray *rows, gpointer user_data);
	gpointer user_data;
} network_mysqld_table;

network_mysqld_index_status *network_mysqld_index_status_init();
void network_mysqld_index_status_free(network_mysqld_index_status *);

void g_list_string_free(gpointer data, gpointer UNUSED_PARAM(user_data));
gboolean g_hash_table_true(gpointer UNUSED_PARAM(key), gpointer UNUSED_PARAM(value), gpointer UNUSED_PARAM(u));

int g_string_lenenc_append(GString *dest, const char *s);

int network_mysqld_con_set_address(network_address *addr, gchar *address);
int network_mysqld_con_connect(network_mysqld *srv, network_socket *con);
int network_mysqld_con_bind(network_mysqld *srv, network_socket *con);

int network_queue_append(network_queue *queue, const char *data, size_t len, int packet_id);
int network_queue_append_chunk(network_queue *queue, GString *chunk);

int network_mysqld_con_send_ok(network_socket *con);
int network_mysqld_con_send_error(network_socket *con, const gchar *errmsg, gsize errmsg_len);
int network_mysqld_con_send_resultset(network_socket *con, GPtrArray *fields, GPtrArray *rows);

retval_t network_mysqld_read(network_mysqld *srv, network_socket *con);
retval_t network_mysqld_write(network_mysqld *srv, network_socket *con);
retval_t network_mysqld_write_len(network_mysqld *srv, network_socket *con, int send_chunks);

int network_mysqld_server_init(network_mysqld *srv, network_socket *con);
int network_mysqld_proxy_init(network_mysqld *srv, network_socket *con);

int network_mysqld_server_connection_init(network_mysqld *srv, network_mysqld_con *con);
int network_mysqld_proxy_connection_init(network_mysqld *srv, network_mysqld_con *con);

network_mysqld *network_mysqld_init(void);
void *network_mysqld_thread(void *);
void network_mysqld_free(network_mysqld *);

network_socket *network_socket_init(void);

network_mysqld_table *network_mysqld_table_init(void);
void network_mysqld_table_free(network_mysqld_table *);

#endif
