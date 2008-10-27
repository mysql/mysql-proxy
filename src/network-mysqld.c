/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/** 
 * @page protocol MySQL Protocol
 *
 * The MySQL Protocol is spilt up into the four phases
 *
 * -# connect
 * -# auth
 * -# query
 * -# and the close state
 *
 * @dot
 * digraph states {
 *   graph [rankdir=LR];
 *   node [fontname=Helvetica, fontsize=10];
 *
 *   connect [ shape=record ];
 *   close [ shape=record ];
 *
 *   subgraph { 
 *     label = "client";
 *     color = black;
 *     rank = same;
 *     node [ style=filled, fillcolor=lightblue ];
 *     connect; 
 *     auth; 
 *     oldauth; 
 *     query; 
 *     local;
 *   }
 *
 *   subgraph { 
 *     label = "server";
 *     rank = same; 
 *     node [ style=filled, fillcolor=orange ];
 *     handshake; 
 *     authres; 
 *     result; 
 *     infile;
 *   }
 *   
 *   subgraph { 
 *     edge [ fontcolor=blue, color=blue, fontsize=10, fontname=Helvetica ];
 *
 *     connect->handshake [ label = "connecting server" ];
 *     auth->authres [ label = "capabilities, password, default-db", URL="http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol#Client_Authentication_Packet" ]; 
 *     oldauth->authres [ label = "scrambled password" ] ; 
 *     query->result [ label = "command (COM_*)", URL="http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol#Command_Packet" ] ;
 *     query->infile [ label = "LOAD DATA INFILE LOCAL" ];
 *     local->result [ label = "file content"];
 *   }
 *
 *   subgraph {
 *     edge [ fontcolor=red, color=red, fontsize=10, fontname=Helvetica ];
 *     handshake->close [ label = "ERR: host denied", URL="http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol#Error_Packet" ];
 *     handshake->auth [ label = "0x10: handshake", URL="http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol#Handshake_Initialization_Packet" ];
 *     authres->oldauth [ label = "EOF: old password reauth" ];
 *     authres->query [ label = "OK: auth done", URL="http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol#OK_Packet" ];
 *     authres->close [ label = "ERR: auth failed", URL="http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol#Error_Packet" ];
 *     result->query [ label = "result for COM_*", URL="http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol#Result_Set_Header_Packet" ] ;
 *     result->close [ label = "COM_QUIT" ];
 *     result->result [ label = "COM_BINLOG_DUMP" ];
 *     infile->local [ label = "EOF: filename" ];
 *   }
 * }
 * @enddot
 * 
 * Unfolded the sequence diagrams of the different use-cases:
 *
 * -# the client connects to the server and waits for data to return @msc
 *   client, backend;
 *   --- [ label = "connect to backend" ];
 *   client->backend  [ label = "INIT" ];
 * @endmsc
 * -# the auth-phase handles the new SHA1-style passwords and the old scramble() passwords 
 *   -# 4.1+ passwords @msc
 *   client, backend;
 *   --- [ label = "authenticate" ];
 *   backend->client [ label = "HANDSHAKE" ];
 *   client->backend [ label = "AUTH" ];
 *   backend->client [ label = "AUTH_RESULT" ];
 * @endmsc
 *   -# pre-4.1 passwords @msc
 *   client, backend;
 *   --- [ label = "authenticate" ];
 *   backend->client [ label = "HANDSHAKE" ];
 *   client->backend [ label = "AUTH" ];
 *   backend->client [ label = "OLD_PASSWORD_SCRAMBLE" ];
 *   client->backend [ label = "OLD_PASSWORD_AUTH" ];
 *   backend->client [ label = "AUTH_RESULT" ];
 * @endmsc
 * -# the query-phase repeats 
 *   -# COM_QUERY and friends @msc
 *   client, backend;
 *   --- [ label = "query result phase" ];
 *   client->backend [ label = "QUERY" ];
 *   backend->client [ label = "QUERY_RESULT" ];
 * @endmsc
 *   -# COM_QUIT @msc
 *   client, backend;
 *   --- [ label = "query result phase" ];
 *   client->backend [ label = "QUERY" ];
 *   backend->client [ label = "connection close" ];
 * @endmsc
 *   -# COM_BINLOG_DUMP @msc
 *   client, backend;
 *   --- [ label = "query result phase" ];
 *   client->backend [ label = "QUERY" ];
 *   backend->client [ label = "QUERY_RESULT" ];
 *   ... [ label = "more binlog entries" ];
 *   backend->client [ label = "QUERY_RESULT"];
 * @endmsc
 */

#include <sys/types.h>

#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h> /* required for FIONREAD on solaris */
#endif

#ifndef _WIN32
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <arpa/inet.h> /** inet_ntoa */
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <netdb.h>
#include <unistd.h>
#else
#include <winsock2.h>
#include <io.h>
#define ioctl ioctlsocket
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#include <glib.h>

#include <mysql.h>
#include <mysqld_error.h>

#include "network-mysqld.h"
#include "network-mysqld-proto.h"
#include "network-mysqld-packet.h"
#include "network-conn-pool.h"
#include "chassis-mainloop.h"
#include "lua-scope.h"
#include "glib-ext.h"

#if defined(HAVE_SYS_SDT_H) && defined(ENABLE_DTRACE)
#include <sys/sdt.h>
#include "proxy-dtrace-provider.h"
#else
#include "disable-dtrace.h"
#endif

#ifdef HAVE_WRITEV
#define USE_BUFFERED_NETIO 
#else
#undef USE_BUFFERED_NETIO 
#endif

#ifdef _WIN32
#define E_NET_CONNRESET WSAECONNRESET
#define E_NET_CONNABORTED WSAECONNABORTED
#define E_NET_WOULDBLOCK WSAEWOULDBLOCK
#define E_NET_INPROGRESS WSAEINPROGRESS
#else
#define E_NET_CONNRESET ECONNRESET
#define E_NET_CONNABORTED ECONNABORTED
#define E_NET_INPROGRESS EINPROGRESS
#if EWOULDBLOCK == EAGAIN
/**
 * some system make EAGAIN == EWOULDBLOCK which would lead to a 
 * error in the case handling
 *
 * set it to -1 as this error should never happen
 */
#define E_NET_WOULDBLOCK -1
#else
#define E_NET_WOULDBLOCK EWOULDBLOCK
#endif
#endif

/**
 * a handy marco for constant strings 
 */
#define C(x) x, sizeof(x) - 1

/**
 * call the cleanup callback for the current connection
 *
 * @param srv    global context
 * @param con    connection context
 *
 * @return       NETWORK_SOCKET_SUCCESS on success
 */
network_socket_retval_t plugin_call_cleanup(chassis *srv, network_mysqld_con *con) {
	NETWORK_MYSQLD_PLUGIN_FUNC(func) = NULL;
	network_socket_retval_t retval = NETWORK_SOCKET_SUCCESS;

	func = con->plugins.con_cleanup;
	
	if (!func) return retval;

	LOCK_LUA(srv->priv->sc);
	retval = (*func)(srv, con);
	UNLOCK_LUA(srv->priv->sc);

	return retval;
}

chassis_private *network_mysqld_priv_init(void) {
	chassis_private *priv;

	priv = g_new0(chassis_private, 1);

	priv->cons = g_ptr_array_new();
	priv->sc = lua_scope_init();
	priv->backends  = network_backends_new();

	return priv;
}

void network_mysqld_priv_shutdown(chassis *chas, chassis_private *priv) {
	if (!priv) return;

	/* network_mysqld_con_free() changes the priv->cons directly
	 *
	 * always free the first element until all are gone 
	 */
	while (0 != priv->cons->len) {
		network_mysqld_con *con = priv->cons->pdata[0];

		plugin_call_cleanup(chas, con);
		network_mysqld_con_free(con);
	}
}

void network_mysqld_priv_free(chassis *chas, chassis_private *priv) {
	if (!priv) return;

	g_ptr_array_free(priv->cons, TRUE);

	network_backends_free(priv->backends);

	lua_scope_free(priv->sc);

	g_free(priv);
}

int network_mysqld_init(chassis *srv) {
	lua_State *L;
	srv->priv_free = network_mysqld_priv_free;
	srv->priv_shutdown = network_mysqld_priv_shutdown;
	srv->priv      = network_mysqld_priv_init();

	/* store the pointer to the chassis in the Lua registry */
	L = srv->priv->sc->L;
	lua_pushlightuserdata(L, (void*)srv);
	lua_setfield(L, LUA_REGISTRYINDEX, "chassis");
	
	return 0;
}


/**
 * create a connection 
 *
 * @param srv    global context
 * @return       a connection context
 */
network_mysqld_con *network_mysqld_con_init() {
	network_mysqld_con *con;

	con = g_new0(network_mysqld_con, 1);

	return con;
}

void network_mysqld_add_connection(chassis *srv, network_mysqld_con *con) {
	con->srv = srv;

	g_ptr_array_add(srv->priv->cons, con);
}

/**
 * free a connection 
 *
 * closes the client and server sockets 
 *
 * @param con    connection context
 */
void network_mysqld_con_free(network_mysqld_con *con) {
	if (!con) return;

	if (con->parse.data && con->parse.data_free) {
		con->parse.data_free(con->parse.data);
	}

	if (con->server) network_socket_free(con->server);
	if (con->client) network_socket_free(con->client);

	/* we are still in the conns-array */

	g_ptr_array_remove_fast(con->srv->priv->cons, con);

	g_free(con);
}

#if 0 
static void dump_str(const char *msg, const unsigned char *s, size_t len) {
	GString *hex;
	size_t i;
		
       	hex = g_string_new(NULL);

	for (i = 0; i < len; i++) {
		g_string_append_printf(hex, "%02x", s[i]);

		if ((i + 1) % 16 == 0) {
			g_string_append(hex, "\n");
		} else {
			g_string_append_c(hex, ' ');
		}

	}

	g_message("(%s): %s", msg, hex->str);

	g_string_free(hex, TRUE);
}
#endif

int network_mysqld_queue_append(network_queue *queue, const char *data, size_t len, int packet_id) {
	unsigned char header[4];
	GString *s;

	network_mysqld_proto_set_header(header, len, packet_id);

	s = g_string_sized_new(len + 4);

	g_string_append_len(s, (gchar *)header, 4);
	g_string_append_len(s, data, len);

	network_queue_append(queue, s);

	return 0;
}


/**
 * create a OK packet and append it to the send-queue
 *
 * @param con             a client socket 
 * @param affected_rows   affected rows 
 * @param insert_id       insert_id 
 * @param server_status   server_status (bitfield of SERVER_STATUS_*) 
 * @param warnings        number of warnings to fetch with SHOW WARNINGS 
 * @return 0
 *
 * @todo move to network_mysqld_proto
 */
int network_mysqld_con_send_ok_full(network_socket *con, guint64 affected_rows, guint64 insert_id, guint16 server_status, guint16 warnings ) {
	GString *packet = g_string_new(NULL);
	network_mysqld_ok_packet_t *ok_packet;

	ok_packet = network_mysqld_ok_packet_new();
	ok_packet->affected_rows = affected_rows;
	ok_packet->insert_id     = insert_id;
	ok_packet->server_status = server_status;
	ok_packet->warnings      = warnings;

	network_mysqld_proto_append_ok_packet(packet, ok_packet);
	
	network_mysqld_queue_append(con->send_queue, packet->str, packet->len, con->packet_id);

	g_string_free(packet, TRUE);
	network_mysqld_ok_packet_free(ok_packet);

	return 0;
}

/**
 * send a simple OK packet
 *
 * - no affected rows
 * - no insert-id
 * - AUTOCOMMIT
 * - no warnings
 *
 * @param con             a client socket 
 */
int network_mysqld_con_send_ok(network_socket *con) {
	return network_mysqld_con_send_ok_full(con, 0, 0, SERVER_STATUS_AUTOCOMMIT, 0);
}

/**
 * send a error packet to the client connection
 *
 * @note the sqlstate has to match the SQL standard. If no matching SQL state is known, leave it at NULL
 *
 * @param con         the client connection
 * @param errmsg      the error message
 * @param errmsg_len  byte-len of the error-message
 * @param errorcode   mysql error-code we want to send
 * @param sqlstate    if none-NULL, 5-char SQL state to send, if NULL, default SQL state is used
 *
 * @return 0 on success
 */
int network_mysqld_con_send_error_full(network_socket *con, const char *errmsg, gsize errmsg_len, guint errorcode, const gchar *sqlstate) {
	GString *packet;
	network_mysqld_err_packet_t *err_packet;

	packet = g_string_sized_new(10 + errmsg_len);
	
	err_packet = network_mysqld_err_packet_new();
	err_packet->errcode = errorcode;
	if (errmsg) g_string_assign_len(err_packet->errmsg, errmsg, errmsg_len);
	if (sqlstate) g_string_assign_len(err_packet->sqlstate, sqlstate, strlen(sqlstate));

	network_mysqld_proto_append_err_packet(packet, err_packet);

	network_mysqld_queue_append(con->send_queue, packet->str, packet->len, con->packet_id);

	network_mysqld_err_packet_free(err_packet);
	g_string_free(packet, TRUE);

	return 0;
}

/**
 * send a error-packet to the client connection
 *
 * errorcode is 1000, sqlstate is NULL
 *
 * @param con         the client connection
 * @param errmsg      the error message
 * @param errmsg_len  byte-len of the error-message
 *
 * @see network_mysqld_con_send_error_full
 */
int network_mysqld_con_send_error(network_socket *con, const char *errmsg, gsize errmsg_len) {
	return network_mysqld_con_send_error_full(con, errmsg, errmsg_len, ER_UNKNOWN_ERROR, NULL);
}

/**
 * read a MySQL packet from the socket
 *
 * the packet is added to the con->recv_queue and contains a full mysql packet
 * with packet-header and everything 
 */
network_socket_retval_t network_mysqld_read(chassis *srv, network_socket *con) {
	GString *packet = NULL;

	/* check if the recv_queue is clean up to now */
	switch (network_socket_read(con)) {
	case NETWORK_SOCKET_WAIT_FOR_EVENT:
		return NETWORK_SOCKET_WAIT_FOR_EVENT;
	case NETWORK_SOCKET_ERROR:
		return NETWORK_SOCKET_ERROR;
	case NETWORK_SOCKET_SUCCESS:
		break;
	case NETWORK_SOCKET_ERROR_RETRY:
		g_error("NETWORK_SOCKET_ERROR_RETRY wasn't expected");
		break;
	}

	/** 
	 * read the packet header (4 bytes)
	 */
	if (con->packet_len == PACKET_LEN_UNSET) {
		GString header;
		char header_str[NET_HEADER_SIZE + 1] = "";

		header.str = header_str;
		header.allocated_len = sizeof(header_str);
		header.len = 0;

		if (!network_queue_peek_string(con->recv_queue_raw, NET_HEADER_SIZE, &header)) {
			/* too small */

			return NETWORK_SOCKET_WAIT_FOR_EVENT;
		}

		con->packet_len = network_mysqld_proto_get_header((unsigned char *)(header_str));
		con->packet_id  = (unsigned char)(header_str[3]); /* packet-id if the next packet */
	}
/* TODO: convert to DTrace probe
	g_debug("[%s] read packet id %d of length %d", G_STRLOC, con->packet_id, con->packet_len + NET_HEADER_SIZE); */
	/* move the packet from the raw queue to the recv-queue */
	if ((packet = network_queue_pop_string(con->recv_queue_raw, con->packet_len + NET_HEADER_SIZE, NULL))) {
		network_queue_append(con->recv_queue, packet);
	} else {
		return NETWORK_SOCKET_WAIT_FOR_EVENT;
	}

	return NETWORK_SOCKET_SUCCESS;
}

network_socket_retval_t network_mysqld_write(chassis *srv, network_socket *con) {
	network_socket_retval_t ret;

	ret = network_socket_write(con, -1);

	return ret;
}

/**
 * call the hooks of the plugins for each state
 *
 * if the plugin doesn't implement a hook, we provide a default operation
 *
 * @param srv      the global context
 * @param con      the connection context
 * @param state    state to handle
 * @return         NETWORK_SOCKET_SUCCESS on success
 */
network_socket_retval_t plugin_call(chassis *srv, network_mysqld_con *con, int state) {
	network_socket_retval_t ret;
	NETWORK_MYSQLD_PLUGIN_FUNC(func) = NULL;

	switch (state) {
	case CON_STATE_INIT:
		func = con->plugins.con_init;

		if (!func) { /* default implementation */
			con->state = CON_STATE_CONNECT_SERVER;
		}
		break;
	case CON_STATE_CONNECT_SERVER:
		func = con->plugins.con_connect_server;

		if (!func) { /* default implementation */
			con->state = CON_STATE_READ_HANDSHAKE;
		}

		break;
	case CON_STATE_SEND_HANDSHAKE:
		func = con->plugins.con_send_handshake;

		if (!func) { /* default implementation */
			con->state = CON_STATE_READ_AUTH;
		}

		break;
	case CON_STATE_READ_HANDSHAKE:
		func = con->plugins.con_read_handshake;

		break;
	case CON_STATE_READ_AUTH:
		func = con->plugins.con_read_auth;

		break;
	case CON_STATE_SEND_AUTH:
		func = con->plugins.con_send_auth;

		if (!func) { /* default implementation */
			con->state = CON_STATE_READ_AUTH_RESULT;
		}
		break;
	case CON_STATE_READ_AUTH_RESULT:
		func = con->plugins.con_read_auth_result;
		break;
	case CON_STATE_SEND_AUTH_RESULT:
		func = con->plugins.con_send_auth_result;

		if (!func) { /* default implementation */
			switch (con->auth_result_state) {
			case MYSQLD_PACKET_OK:
				con->state = CON_STATE_READ_QUERY;
				break;
			case MYSQLD_PACKET_ERR:
				con->state = CON_STATE_ERROR;
				break;
			case MYSQLD_PACKET_EOF:
				/**
				 * the MySQL 4.0 hash in a MySQL 4.1+ connection
				 */
				con->state = CON_STATE_READ_AUTH_OLD_PASSWORD;
				break;
			default:
				g_error("%s.%d: unexpected state for SEND_AUTH_RESULT: %02x", 
						__FILE__, __LINE__,
						con->auth_result_state);
			}
		}
		break;
	case CON_STATE_READ_AUTH_OLD_PASSWORD: {
		/** move the packet to the send queue */
		GString *packet;
		GList *chunk;
		network_socket *recv_sock, *send_sock;

		recv_sock = con->client;
		send_sock = con->server;

		if (NULL == con->server) {
			/**
			 * we have to auth against same backend as we did before
			 * but the user changed it
			 */

			g_message("%s.%d: (lua) read-auth-old-password failed as backend_ndx got reset.", __FILE__, __LINE__);

			network_mysqld_con_send_error(con->client, C("(lua) read-auth-old-password failed as backend_ndx got reset."));
			con->state = CON_STATE_SEND_ERROR;
			break;
		}

		chunk = recv_sock->recv_queue->chunks->head;
		packet = chunk->data;

		/* we aren't finished yet */
		if (packet->len != recv_sock->packet_len + NET_HEADER_SIZE) return NETWORK_SOCKET_SUCCESS;

		network_queue_append(send_sock->send_queue, packet);

		recv_sock->packet_len = PACKET_LEN_UNSET;
		g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

		/**
		 * send it out to the client 
		 */
		con->state = CON_STATE_SEND_AUTH_OLD_PASSWORD;
		break; }
	case CON_STATE_SEND_AUTH_OLD_PASSWORD:
		/**
		 * data is at the server, read the response next 
		 */
		con->state = CON_STATE_READ_AUTH_RESULT;
		break;
	case CON_STATE_READ_QUERY:
		func = con->plugins.con_read_query;
		break;
	case CON_STATE_READ_QUERY_RESULT:
		func = con->plugins.con_read_query_result;
		break;
	case CON_STATE_SEND_QUERY_RESULT:
		func = con->plugins.con_send_query_result;

		if (!func) { /* default implementation */
			con->state = CON_STATE_READ_QUERY;
		}
		break;
	case CON_STATE_ERROR:
		g_debug("%s.%d: not executing plugin function in state CON_STATE_ERROR", __FILE__, __LINE__);
		return NETWORK_SOCKET_SUCCESS;
	default:
		g_error("%s.%d: unhandled state: %d", 
				__FILE__, __LINE__,
				state);
	}
	if (!func) return NETWORK_SOCKET_SUCCESS;

	LOCK_LUA(srv->priv->sc);
	ret = (*func)(srv, con);
	UNLOCK_LUA(srv->priv->sc);

	return ret;
}

/**
 * reset the command-response parsing
 *
 * some commands needs state information and we have to 
 * reset the parsing as soon as we add a new command to the send-queue
 */
void network_mysqld_con_reset_command_response_state(network_mysqld_con *con) {
	con->parse.command = -1;
	if (con->parse.data && con->parse.data_free) {
		con->parse.data_free(con->parse.data);

		con->parse.data = NULL;
		con->parse.data_free = NULL;
	}
}


/**
 * handle the different states of the MySQL protocol
 *
 * @param event_fd     fd on which the event was fired
 * @param events       the event that was fired
 * @param user_data    the connection handle
 */
void network_mysqld_con_handle(int event_fd, short events, void *user_data) {
	guint ostate;
	network_mysqld_con *con = user_data;
	chassis *srv = con->srv;
	int retval;

	g_assert(srv);
	g_assert(con);

	if (events == EV_READ) {
		int b = -1;

		if (ioctl(event_fd, FIONREAD, &b)) {
			g_critical("ioctl(%d, FIONREAD, ...) failed: %s", event_fd, g_strerror(errno));

			con->state = CON_STATE_ERROR;
		} else if (b != 0) {
			if (con->client && event_fd == con->client->fd) {
				con->client->to_read = b;
			} else if (con->server && event_fd == con->server->fd) {
				con->server->to_read = b;
			} else {
				g_error("%s.%d: neither nor", __FILE__, __LINE__);
			}
		} else {
			if (con->client && event_fd == con->client->fd) {
				/* the client closed the connection, let's keep the server side open */
				con->state = CON_STATE_CLOSE_CLIENT;
			} else {
				/* server side closed on use, oops, close both sides */
				con->state = CON_STATE_ERROR;
			}
		}
	}

#define WAIT_FOR_EVENT(ev_struct, ev_type, timeout) \
	event_set(&(ev_struct->event), ev_struct->fd, ev_type, network_mysqld_con_handle, user_data); \
	event_base_set(srv->event_base, &(ev_struct->event));\
	event_add(&(ev_struct->event), timeout);

	/**
	 * loop on the same connection as long as we don't end up in a stable state
	 */
	do {
		ostate = con->state;
		MYSQLPROXY_STATE_CHANGE(event_fd, events, con->state);
		switch (con->state) {
		case CON_STATE_ERROR:
			/* we can't go on, close the connection */
			{
				gchar *which_connection = "a"; /* some connection, don't know yet */
				if (con->server && event_fd == con->server->fd) {
					which_connection = "server";
				} else if (con->client && event_fd == con->client->fd) {
					which_connection = "client";
				}
				g_debug("[%s]: error on %s connection (fd: %d event: %d). closing client connection.",
						G_STRLOC, which_connection,	event_fd, events);
			}
			plugin_call_cleanup(srv, con);
			network_mysqld_con_free(con);

			con = NULL;

			return;
		case CON_STATE_CLOSE_CLIENT:
			/* the server connection is still fine, 
			 * let's keep it open for reuse */
			plugin_call_cleanup(srv, con);
			network_mysqld_con_free(con);

			con = NULL;

			return;
		case CON_STATE_INIT:
			/* if we are a proxy ask the remote server for the hand-shake packet 
			 * if not, we generate one */
			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			default:
				/**
				 * no luck, let's close the connection
				 */
				g_critical("%s.%d: plugin_call(CON_STATE_INIT) != NETWORK_SOCKET_SUCCESS", __FILE__, __LINE__);

				con->state = CON_STATE_ERROR;
				
				break;
			}

			break;
		case CON_STATE_CONNECT_SERVER:
			switch ((retval = plugin_call(srv, con, con->state))) {
			case NETWORK_SOCKET_SUCCESS:

				/**
				 * hmm, if this is success and we have something in the clients send-queue
				 * we just send it out ... who needs a server ? */

				if ((con->client != NULL && con->client->send_queue->chunks->length > 0) && 
				     con->server == NULL) {
					/* we want to send something to the client */

					con->state = CON_STATE_SEND_HANDSHAKE;
				} else {
					g_assert(con->server);
				}

				break;
			case NETWORK_SOCKET_ERROR_RETRY:
				if (con->server) {
					/**
					 * we have a server connection waiting to begin writable
					 */
					WAIT_FOR_EVENT(con->server, EV_WRITE, NULL);
					return;
				} else {
					/* try to get a connection to another backend,
					 *
					 * setting ostate = CON_STATE_INIT is a hack to make sure
					 * the loop is coming back to this function again */
					ostate = CON_STATE_INIT;
				}

				break;
			case NETWORK_SOCKET_ERROR:
				/**
				 * connecting failed and no option to retry
				 *
				 * close the connection
				 */
				con->state = CON_STATE_SEND_ERROR;
				break;
			default:
				g_critical("%s: hook for CON_STATE_CONNECT_SERVER return invalid return code: %d", 
						G_STRLOC, 
						retval);

				con->state = CON_STATE_ERROR;
				
				break;
			}

			break;
		case CON_STATE_READ_HANDSHAKE: {
			/**
			 * read auth data from the remote mysql-server 
			 */
			network_socket *recv_sock;
			recv_sock = con->server;
			g_assert(events == 0 || event_fd == recv_sock->fd);

			switch (network_mysqld_read(srv, recv_sock)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				/* call us again when you have a event */
				WAIT_FOR_EVENT(con->server, EV_READ, NULL);

				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				g_critical("%s.%d: network_mysqld_read(CON_STATE_READ_HANDSHAKE) returned an error", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}

			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_ERROR:
				/**
				 * we couldn't understand the pack from the server 
				 * 
				 * we have something in the queue and will send it to the client
				 * and close the connection afterwards
				 */
				
				con->state = CON_STATE_SEND_ERROR;

				break;
			default:
				g_critical("%s.%d: ...", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}

			if (recv_sock->challenge &&
			    recv_sock->challenge->server_version > 50113 && recv_sock->challenge->server_version < 50118) {
				/**
				 * Bug #25371
				 *
				 * COM_CHANGE_USER returns 2 ERR packets instead of one
				 *
				 * we can auto-correct the issue if needed and remove the second packet
				 * Some clients handle this issue and expect a double ERR packet.
				 */

				con->state = CON_STATE_ERROR;
			}
	
			break; }
		case CON_STATE_SEND_HANDSHAKE: 
			/* send the hand-shake to the client and wait for a response */

			switch (network_mysqld_write(srv, con->client)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_WRITE, NULL);
				
				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				/**
				 * writing failed, closing connection
				 */
				con->state = CON_STATE_ERROR;
				break;
			}

			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			default:
				g_critical("%s.%d: plugin_call(CON_STATE_SEND_HANDSHAKE) != NETWORK_SOCKET_SUCCESS", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}

			break;
		case CON_STATE_READ_AUTH: {
			/* read auth from client */
			network_socket *recv_sock;

			recv_sock = con->client;

			g_assert(events == 0 || event_fd == recv_sock->fd);

			switch (network_mysqld_read(srv, recv_sock)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_READ, NULL);

				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				g_critical("%s.%d: network_mysqld_read(CON_STATE_READ_AUTH) returned an error", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}
			
			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_ERROR:
				con->state = CON_STATE_SEND_ERROR;
				break;
			default:
				g_critical("%s.%d: plugin_call(CON_STATE_READ_AUTH) != NETWORK_SOCKET_SUCCESS", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}

			break; }
		case CON_STATE_SEND_AUTH:
			/* send the auth-response to the server */
			switch (network_mysqld_write(srv, con->server)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->server, EV_WRITE, NULL);

				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				/* might be a connection close, we should just close the connection and be happy */
				con->state = CON_STATE_ERROR;

				break;
			}
			
			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			default:
				g_critical("%s.%d: plugin_call(CON_STATE_SEND_AUTH) != NETWORK_SOCKET_SUCCESS", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}

			break;
		case CON_STATE_READ_AUTH_RESULT: {
			/* read the auth result from the server */
			network_socket *recv_sock;
			GList *chunk;
			GString *packet;
			recv_sock = con->server;

			g_assert(events == 0 || event_fd == recv_sock->fd);

			switch (network_mysqld_read(srv, recv_sock)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->server, EV_READ, NULL);
				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				g_critical("%s.%d: network_mysqld_read(CON_STATE_READ_AUTH_RESULT) returned an error", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}
			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			/**
			 * depending on the result-set we have different exit-points
			 * - OK  -> READ_QUERY
			 * - EOF -> (read old password hash) 
			 * - ERR -> ERROR
			 */
			chunk = recv_sock->recv_queue->chunks->head;
			packet = chunk->data;
			g_assert(packet);
			g_assert(packet->len > NET_HEADER_SIZE);

			con->auth_result_state = packet->str[NET_HEADER_SIZE];

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			default:
				g_critical("%s.%d: plugin_call(CON_STATE_READ_AUTH_RESULT) != NETWORK_SOCKET_SUCCESS", __FILE__, __LINE__);

				con->state = CON_STATE_ERROR;
				break;
			}

			break; }
		case CON_STATE_SEND_AUTH_RESULT: {
			/* send the hand-shake to the client and wait for a response */

			switch (network_mysqld_write(srv, con->client)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_WRITE, NULL);
				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				g_debug("%s.%d: network_mysqld_write(CON_STATE_SEND_AUTH_RESULT) returned an error", __FILE__, __LINE__);

				con->state = CON_STATE_ERROR;
				break;
			}
			
			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			default:
				g_critical("%s.%d: ...", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}
				
			break; }
		case CON_STATE_READ_AUTH_OLD_PASSWORD: 
			/* read auth from client */
			switch (network_mysqld_read(srv, con->client)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_READ, NULL);

				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				g_critical("%s.%d: network_mysqld_read(CON_STATE_READ_AUTH_OLD_PASSWORD) returned an error", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				return;
			}
			
			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			default:
				g_critical("%s.%d: plugin_call(CON_STATE_READ_AUTH_OLD_PASSWORD) != NETWORK_SOCKET_SUCCESS", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}

			break; 
		case CON_STATE_SEND_AUTH_OLD_PASSWORD:
			/* send the auth-response to the server */
			switch (network_mysqld_write(srv, con->server)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->server, EV_WRITE, NULL);

				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				/* might be a connection close, we should just close the connection and be happy */
				g_debug("%s.%d: network_mysqld_write(CON_STATE_SEND_AUTH_OLD_PASSWORD) returned an error", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}
			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			default:
				g_critical("%s.%d: plugin_call(CON_STATE_SEND_AUTH_OLD_PASSWORD) != NETWORK_SOCKET_SUCCESS", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				break;
			}

			break;

		case CON_STATE_READ_QUERY: {
			network_socket *recv_sock;
			recv_sock = con->client;

			g_assert(events == 0 || event_fd == recv_sock->fd);

			switch (network_mysqld_read(srv, recv_sock)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_READ, NULL);
				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				g_critical("%s.%d: network_mysqld_read(CON_STATE_READ_QUERY) returned an error", __FILE__, __LINE__);
				con->state = CON_STATE_ERROR;
				return;
			}
			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			default:
				g_critical("%s.%d: plugin_call(CON_STATE_READ_QUERY) failed", __FILE__, __LINE__);

				con->state = CON_STATE_ERROR;
				break;
			}

			network_mysqld_con_reset_command_response_state(con);

			break; }
		case CON_STATE_SEND_QUERY:
			/* send the query to the server */
			if (con->server->send_queue->offset == 0) {
				/* only parse the packets once */
				network_packet packet;
				GList *chunk;

				g_assert(con->server->send_queue->chunks);
				chunk = con->server->send_queue->chunks->head;

				packet.data = chunk->data;
				packet.offset = 0;

				/* only parse once and don't care about the blocking read */
				if (con->parse.command == COM_QUERY) {
					network_mysqld_com_query_result_track_state(&packet, con->parse.data);
				} else if (con->is_overlong_packet) {
					/* the last packet was a over-long packet
					 * this is the same command, just more data */
	
					if (con->parse.len != PACKET_LEN_MAX) {
						con->is_overlong_packet = 0;
					}
	
				} else {
					con->parse.command = packet.data->str[4];
	
					if (con->parse.len == PACKET_LEN_MAX) {
						con->is_overlong_packet = 1;
					}
		
					/* init the parser for the commands */
					switch (con->parse.command) {
					case COM_QUERY:
					case COM_PROCESS_INFO:
					case COM_STMT_EXECUTE:
						con->parse.data = network_mysqld_com_query_result_new();
						con->parse.data_free = (GDestroyNotify)network_mysqld_com_query_result_free;
						break;
					case COM_STMT_PREPARE:
						con->parse.data = network_mysqld_com_stmt_prepare_result_new();
						con->parse.data_free = (GDestroyNotify)network_mysqld_com_init_db_result_free;
						break;
					case COM_INIT_DB:
						con->parse.data = network_mysqld_com_init_db_result_new();
						con->parse.data_free = (GDestroyNotify)network_mysqld_com_init_db_result_free;

						network_mysqld_com_init_db_result_track_state(&packet, con->parse.data);

						break;
					default:
						break;
					}
				}
			}
	
			switch (network_socket_write(con->server, 1)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->server, EV_WRITE, NULL);
				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				g_debug("%s.%d: network_mysqld_write(CON_STATE_SEND_QUERY) returned an error", __FILE__, __LINE__);

				/**
				 * write() failed, close the connections 
				 */
				con->state = CON_STATE_ERROR;
				break;
			}
			
			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			/* in case we are waiting for LOAD DATA LOCAL INFILE data on this connection,
			 we need to read the sentinel zero-length packet, too. Otherwise we will block
			 this connection. Bug#37404
			 */
			if (con->is_overlong_packet || con->in_load_data_local_state) {
				/* the last packet of LOAD DATA LOCAL INFILE is zero-length, signalling "no more data following" */
				if (con->parse.len == 0) {
					con->state = CON_STATE_READ_QUERY_RESULT;
					con->in_load_data_local_state = FALSE;
				} else {
					con->state = CON_STATE_READ_QUERY;
				}
				break;
			}

			/* some statements don't have a server response */
			switch (con->parse.command) {
			case COM_STMT_SEND_LONG_DATA: /* not acked */
			case COM_STMT_CLOSE:
				con->state = CON_STATE_READ_QUERY;
				break;
			case COM_QUERY:
				if (network_mysqld_com_query_result_is_load_data(con->parse.data)) {
					con->state = CON_STATE_READ_QUERY;
				} else {
					con->state = CON_STATE_READ_QUERY_RESULT;
				}
				break;
			default:
				con->state = CON_STATE_READ_QUERY_RESULT;
				break;
			}
				
			break; 
		case CON_STATE_READ_QUERY_RESULT: 
			/* read all packets of the resultset 
			 *
			 * depending on the backend we may forward the data to the client right away
			 */
			do {
				network_socket *recv_sock;

				recv_sock = con->server;

				g_assert(events == 0 || event_fd == recv_sock->fd);

				switch (network_mysqld_read(srv, recv_sock)) {
				case NETWORK_SOCKET_SUCCESS:
					break;
				case NETWORK_SOCKET_WAIT_FOR_EVENT:
					WAIT_FOR_EVENT(con->server, EV_READ, NULL);
					return;
				case NETWORK_SOCKET_ERROR_RETRY:
				case NETWORK_SOCKET_ERROR:
					g_critical("%s.%d: network_mysqld_read(CON_STATE_READ_QUERY_RESULT) returned an error", __FILE__, __LINE__);
					con->state = CON_STATE_ERROR;
					break;
				}
				if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

				switch (plugin_call(srv, con, con->state)) {
				case NETWORK_SOCKET_SUCCESS:
					/* if we don't need the resultset, forward it to the client */
					if (!con->resultset_is_finished && !con->resultset_is_needed) {
						/* check how much data we have in the queue waiting, no need to try to send 5 bytes */
						if (con->client->send_queue->len > 64 * 1024) {
							con->state = CON_STATE_SEND_QUERY_RESULT;
						}
					}
					break;
				case NETWORK_SOCKET_ERROR:
					/* something nasty happend, let's close the connection */
					con->state = CON_STATE_ERROR;
					break;
				default:
					g_critical("%s.%d: ...", __FILE__, __LINE__);
					con->state = CON_STATE_ERROR;
					break;
				}


			} while (con->state == CON_STATE_READ_QUERY_RESULT);
	
			break; 
		case CON_STATE_SEND_QUERY_RESULT:
			/**
			 * send the query result-set to the client */
			switch (network_mysqld_write(srv, con->client)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_WRITE, NULL);
				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				/**
				 * client is gone away
				 *
				 * close the connection and clean up
				 */
				con->state = CON_STATE_ERROR;
				break;
			}

			/* if the write failed, don't call the plugin handlers */
			if (con->state != ostate) break; /* the state has changed (e.g. CON_STATE_ERROR) */

			/* in case we havn't read the full resultset from the server yet, go back and read more
			 */
			if (!con->resultset_is_finished) {
				con->state = CON_STATE_READ_QUERY_RESULT;
				break;
			}

			switch (plugin_call(srv, con, con->state)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			default:
				con->state = CON_STATE_ERROR;
				break;
			}

			break;
		case CON_STATE_SEND_ERROR:
			/**
			 * send error to the client
			 * and close the connections afterwards
			 *  */
			switch (network_mysqld_write(srv, con->client)) {
			case NETWORK_SOCKET_SUCCESS:
				break;
			case NETWORK_SOCKET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_WRITE, NULL);
				return;
			case NETWORK_SOCKET_ERROR_RETRY:
			case NETWORK_SOCKET_ERROR:
				g_critical("%s.%d: network_mysqld_write(CON_STATE_SEND_ERROR) returned an error", __FILE__, __LINE__);

				con->state = CON_STATE_ERROR;
				break;
			}
				
			con->state = CON_STATE_ERROR;

			break;
		}

		event_fd = -1;
		events   = 0;
	} while (ostate != con->state);

	return;
}

/**
 * accept a connection
 *
 * event handler for listening connections
 *
 * @param event_fd     fd on which the event was fired
 * @param events       the event that was fired
 * @param user_data    the listening connection handle
 * 
 */
void network_mysqld_con_accept(int event_fd, short events, void *user_data) {
	network_mysqld_con *listen_con = user_data;
	network_mysqld_con *client_con;
	socklen_t addr_len;
	struct sockaddr_in ipv4;
	int fd;

	g_assert(events == EV_READ);
	g_assert(listen_con->server);

	addr_len = sizeof(struct sockaddr_in);

	if (-1 == (fd = accept(event_fd, (struct sockaddr *)&ipv4, &addr_len))) {
		return ;
	}


	/* looks like we open a client connection */
	client_con = network_mysqld_con_init();
	network_mysqld_add_connection(listen_con->srv, client_con);

	client_con->client = network_socket_init();
	client_con->client->addr.addr.ipv4 = ipv4;
	client_con->client->addr.len = addr_len;
	client_con->client->fd   = fd;

	network_socket_set_non_blocking(client_con->client);

	
	if (network_address_resolve_address(&(client_con->client->addr)) != NETWORK_SOCKET_SUCCESS) {
        g_message("%s.%d: resolving address failed, closing connection", __FILE__, __LINE__);
		network_mysqld_con_free(client_con);
        return;
	}

	/**
	 * inherit the config to the new connection 
	 */

	client_con->plugins = listen_con->plugins;
	client_con->config  = listen_con->config;
	
	network_mysqld_con_handle(-1, 0, client_con);

	return;
}

/**
 * @todo move to network_mysqld_proto
 */
int network_mysqld_con_send_resultset(network_socket *con, GPtrArray *fields, GPtrArray *rows) {
	GString *s;
	gsize i, j;

	g_assert(fields->len > 0 && fields->len < 251);

	s = g_string_new(NULL);

	/* - len = 99
	 *  \1\0\0\1 
	 *    \1 - one field
	 *  \'\0\0\2 
	 *    \3def 
	 *    \0 
	 *    \0 
	 *    \0 
	 *    \21@@version_comment 
	 *    \0            - org-name
	 *    \f            - filler
	 *    \10\0         - charset
	 *    \34\0\0\0     - length
	 *    \375          - type 
	 *    \1\0          - flags
	 *    \37           - decimals
	 *    \0\0          - filler 
	 *  \5\0\0\3 
	 *    \376\0\0\2\0
	 *  \35\0\0\4
	 *    \34MySQL Community Server (GPL)
	 *  \5\0\0\5
	 *    \376\0\0\2\0
	 */

	g_string_append_c(s, fields->len); /* the field-count */
	network_mysqld_queue_append(con->send_queue, s->str, s->len, con->packet_id++);

	for (i = 0; i < fields->len; i++) {
		MYSQL_FIELD *field = fields->pdata[i];
		
		g_string_truncate(s, 0);

		network_mysqld_proto_append_lenenc_string(s, field->catalog ? field->catalog : "def");   /* catalog */
		network_mysqld_proto_append_lenenc_string(s, field->db ? field->db : "");                /* database */
		network_mysqld_proto_append_lenenc_string(s, field->table ? field->table : "");          /* table */
		network_mysqld_proto_append_lenenc_string(s, field->org_table ? field->org_table : "");  /* org_table */
		network_mysqld_proto_append_lenenc_string(s, field->name ? field->name : "");            /* name */
		network_mysqld_proto_append_lenenc_string(s, field->org_name ? field->org_name : "");    /* org_name */

		g_string_append_c(s, '\x0c');                  /* length of the following block, 12 byte */
		g_string_append_len(s, "\x08\x00", 2);         /* charset */
		g_string_append_c(s, (field->length >> 0) & 0xff); /* len */
		g_string_append_c(s, (field->length >> 8) & 0xff); /* len */
		g_string_append_c(s, (field->length >> 16) & 0xff); /* len */
		g_string_append_c(s, (field->length >> 24) & 0xff); /* len */
		g_string_append_c(s, field->type);             /* type */
		g_string_append_c(s, field->flags & 0xff);     /* flags */
		g_string_append_c(s, (field->flags >> 8) & 0xff); /* flags */
		g_string_append_c(s, 0);                       /* decimals */
		g_string_append_len(s, "\x00\x00", 2);         /* filler */
#if 0
		/* this is in the docs, but not on the network */
		network_mysqld_proto_append_lenenc_string(s, field->def);         /* default-value */
#endif
		network_mysqld_queue_append(con->send_queue, s->str, s->len, con->packet_id++);
	}

	g_string_truncate(s, 0);
	
	/* EOF */	
	g_string_append_len(s, "\xfe", 1); /* EOF */
	g_string_append_len(s, "\x00\x00", 2); /* warning count */
	g_string_append_len(s, "\x02\x00", 2); /* flags */
	
	network_mysqld_queue_append(con->send_queue, s->str, s->len, con->packet_id++);

	for (i = 0; i < rows->len; i++) {
		GPtrArray *row = rows->pdata[i];

		g_string_truncate(s, 0);

		for (j = 0; j < row->len; j++) {
			network_mysqld_proto_append_lenenc_string(s, row->pdata[j]);
		}
		network_mysqld_queue_append(con->send_queue, s->str, s->len, con->packet_id++);
	}

	g_string_truncate(s, 0);

	/* EOF */	
	g_string_append_len(s, "\xfe", 1); /* EOF */
	g_string_append_len(s, "\x00\x00", 2); /* warning count */
	g_string_append_len(s, "\x02\x00", 2); /* flags */

	network_mysqld_queue_append(con->send_queue, s->str, s->len, con->packet_id++);

	g_string_free(s, TRUE);

	return 0;
}


