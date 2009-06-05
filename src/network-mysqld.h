/* $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ */
 

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
#include "network-backend.h"
#include "lua-registry-keys.h"

typedef struct network_mysqld_con network_mysqld_con; /* forward declaration */

/**
 * some plugins don't use the global "chas" pointer 
 */
#define NETWORK_MYSQLD_PLUGIN_FUNC(x) network_socket_retval_t (*x)(chassis *, network_mysqld_con *)
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

/**
 * A structure containing the parsed packet for a command packet as well as the common parts necessary to find the correct
 * packet parsing function.
 * 
 * The correct parsing function is chose by looking at both the current state as well as the command in this structure.
 * 
 * @todo Currently the plugins are responsible for setting the first two fields of this structure. We have to investigate
 * how we can refactor this into a more generic way.
 */
struct network_mysqld_con_parse {
	guint32 len;						/**< The overall length of the packet */
	enum enum_server_command command;	/**< The command indicator from the MySQL Protocol */

	gpointer data;						/**< An opaque pointer to a parsed command structure */
	void (*data_free)(gpointer);		/**< A function pointer to the appropriate "free" function of data */
};

typedef enum { 
	CON_STATE_INIT = 0, 
	CON_STATE_CONNECT_SERVER = 1, 
	CON_STATE_READ_HANDSHAKE = 2, 
	CON_STATE_SEND_HANDSHAKE = 3, 
	CON_STATE_READ_AUTH = 4, 
	CON_STATE_SEND_AUTH = 5, 
	CON_STATE_READ_AUTH_RESULT = 6,
	CON_STATE_SEND_AUTH_RESULT = 7,
	CON_STATE_READ_AUTH_OLD_PASSWORD = 8,
	CON_STATE_SEND_AUTH_OLD_PASSWORD = 9,
	CON_STATE_READ_QUERY = 10,
	CON_STATE_SEND_QUERY = 11,
	CON_STATE_READ_QUERY_RESULT = 12,
	CON_STATE_SEND_QUERY_RESULT = 13,
	
	CON_STATE_CLOSE_CLIENT = 14,
	CON_STATE_SEND_ERROR = 15,
	CON_STATE_ERROR = 16,

	CON_STATE_CLOSE_SERVER = 17
} network_mysqld_con_state_t;

/**
 * Encapsulates the state and callback functions for a MySQL protocol-based connection to and from MySQL Proxy.
 * 
 * New connection structures are created by the function responsible for handling the accept on a listen socket, which
 * also is a network_mysqld_con structure, but only has a server set - there is no "client" for connections that we listen on.
 * 
 * The chassis itself does not listen on any sockets, this is left to each plugin. Plugins are free to create any number of
 * connections to listen on, but most of them will only create one and reuse the network_mysqld_con_accept function to set up an
 * incoming connection.
 * 
 * Each plugin can register callbacks for the various states in the MySQL Protocol, these are set in the member plugins.
 * A plugin is not required to implement any callbacks at all, but only those that it wants to customize. Callbacks that
 * are not set, will cause the MySQL Proxy core to simply forward the received data.
 */
struct network_mysqld_con {
	/**
	 * The current/next state of this connection.
	 * 
	 * When the protocol state machine performs a transition, this variable will contain the next state,
	 * otherwise, while performing the action at state, it will be set to the connection's current state
	 * in the MySQL protocol.
	 * 
	 * Plugins may update it in a callback to cause an arbitrary state transition, however, this may result
	 * reaching an invalid state leading to connection errors.
	 * 
	 * @see network_mysqld_con_handle
	 */
	network_mysqld_con_state_t state;

	/**
	 * The server side of the connection as it pertains to the low-level network implementation.
	 */
	network_socket *server
	/**
	 * The client side of the connection as it pertains to the low-level network implementation.
	 */
	network_socket *client;

	/**
	 * A boolean flag indicating that the data sent was larger that the max_packet_size.
	 * 
	 * This is used for commands that need to send large amounts of data, e.g. blobs, to correctly
	 * parse the following packet received (which will not appear to have a valid packet header otherwise).
	 */
	int is_overlong_packet;

	/**
	 * Function pointers to the plugin's callbacks.
	 * 
	 * Plugins don't need set any of these, but if unset, the plugin will not have the opportunity to
	 * alter the behavior of the corresponding protocol state.
	 * 
	 * @note In theory you could use functions from different plugins to handle the various states, but there is no guarantee that
	 * this will work. Generally the plugins will assume that config is their own chassis_plugin_config (a plugin-private struct)
	 * and violating this constraint may lead to a crash.
	 * @see chassis_plugin_config
	 */
	network_mysqld_hooks plugins;
	
	/**
	 * A pointer to a plugin-private struct describing configuration parameters.
	 * 
	 * @note The actual struct definition used is private to each plugin.
	 */
	chassis_plugin_config *config;

	/**
	 * A pointer back to the global, singleton chassis structure.
	 */
	chassis *srv; /* our srv object */

	/**
	 * A boolean flag indicating that this connection should only be used to accept incoming connections.
	 * 
	 * It does not follow the MySQL protocol by itself and its client network_socket will always be NULL.
	 */
	int is_listen_socket;

	/**
	 * An integer indicating the result received from a server after sending an authentication request.
	 * 
	 * This is used to differentiate between the old, pre-4.1 authentication and the new, 4.1+ one based on the response.
	 */
	guint8 auth_result_state;

	/** Flag indicating if we the plugin doesn't need the resultset itself.
	 * 
	 * If set to TRUE, the plugin needs to see the entire resultset and we will buffer it.
	 * If set to FALSE, the plugin is not interested in the content of the resultset and we'll
	 * try to forward the packets to the client directly, even before the full resultset is parsed.
	 */
	gboolean resultset_is_needed;
	/**
	 * Flag indicating whether we have seen all parts belonging to one resultset.
	 */
	gboolean resultset_is_finished;

	/**
	 * Flag indicating that we are processing packets from a LOAD DATA LOCAL command.
	 */
	gboolean in_load_data_local_state;
	/**
	 * Flag indicating that we have received a COM_QUIT command.
	 * 
	 * This is mainly used to differentiate between the case where the server closed the connection because of some error
	 * or if the client asked it to close its side of the connection.
	 * MySQL Proxy would report spurious errors for the latter case, if we failed to track this command.
	 */
	gboolean com_quit_seen;

	/**
	 * Contains the parsed packet.
	 */
	struct network_mysqld_con_parse parse;

	/**
	 * An opaque pointer to a structure describing extra connection state needed by the plugin.
	 * 
	 * The content and meaning is completely up to each plugin and the chassis will not access this in any way.
	 */
	void *plugin_con_state;
};



NETWORK_API void g_list_string_free(gpointer data, gpointer UNUSED_PARAM(user_data));
NETWORK_API gboolean g_hash_table_true(gpointer UNUSED_PARAM(key), gpointer UNUSED_PARAM(value), gpointer UNUSED_PARAM(u));

NETWORK_API network_mysqld_con *network_mysqld_con_init(void) G_GNUC_DEPRECATED;
NETWORK_API network_mysqld_con *network_mysqld_con_new(void);
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
