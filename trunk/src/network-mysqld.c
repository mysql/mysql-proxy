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

#include <sys/types.h>

#ifdef HAVE_SYS_FILIO_H
/**
 * required for FIONREAD on solaris
 */
#include <sys/filio.h>
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

#include <glib.h>

#include <mysql.h>
#include <mysqld_error.h>

#include "network-mysqld.h"
#include "network-mysqld-proto.h"
#include "network-conn-pool.h"

#ifdef _WIN32
extern volatile int agent_shutdown;
#else
extern volatile sig_atomic_t agent_shutdown;
#endif

#ifdef _WIN32
#define E_NET_CONNRESET WSAECONNRESET
#define E_NET_CONNABORTED WSAECONNABORTED
#define E_NET_WOULDBLOCK WSAEWOULDBLOCK
#else
#define E_NET_CONNRESET ECONNRESET
#define E_NET_CONNABORTED ECONNABORTED
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
 * @return       RET_SUCCESS on success
 */
retval_t plugin_call_cleanup(network_mysqld *srv, network_mysqld_con *con) {
	NETWORK_MYSQLD_PLUGIN_FUNC(func) = NULL;

	func = con->plugins.con_cleanup;
	
	if (!func) return RET_SUCCESS;

	return (*func)(srv, con);
}


/**
 * create a connection 
 *
 * @param srv    global context
 * @return       a connection context
 */
network_mysqld_con *network_mysqld_con_init(network_mysqld *srv) {
	network_mysqld_con *con;

	con = g_new0(network_mysqld_con, 1);

	con->srv = srv;

	g_ptr_array_add(srv->cons, con);

	return con;
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

	if (con->server) network_socket_free(con->server);
	if (con->client) network_socket_free(con->client);

	/* we are still in the conns-array */

	g_ptr_array_remove_fast(con->srv->cons, con);

	g_free(con);
}


/**
 * type-safe free()-ing of the internal SQL tables
 *
 * @param s    a network_mysqld_tables 
 * @see g_hash_table_new_full()
 */
static void network_mysqld_tables_free_void(void *s) {
	network_mysqld_table_free(s);
}

/**
 * create a global context
 *
 * @see network_mysqld_tables_free_void()
 */
network_mysqld *network_mysqld_init() {
	network_mysqld *m;

	m = g_new0(network_mysqld, 1);

	m->event_base  = NULL;

	m->tables      = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, network_mysqld_tables_free_void);
	
	m->cons        = g_ptr_array_new();
	
	return m;
}

/**
 * init libevent
 *
 * kqueue has to be called after the fork() of daemonize
 *
 * @param m      global context
 */
void network_mysqld_init_libevent(network_mysqld *m) {
	m->event_base  = event_init();
}

/**
 * free the global scope
 *
 * closes all open connections, cleans up all plugins
 *
 * @param m      global context
 */
void network_mysqld_free(network_mysqld *m) {
	guint i;

	if (!m) return;

	for (i = 0; i < m->cons->len; i++) {
		network_mysqld_con *con = m->cons->pdata[i];

		plugin_call_cleanup(m, con);
		network_mysqld_con_free(con);
	}

	g_ptr_array_free(m->cons, TRUE);

	g_hash_table_destroy(m->tables);

	if (m->config.proxy.backend_addresses) {
		for (i = 0; m->config.proxy.backend_addresses[i]; i++) {
			g_free(m->config.proxy.backend_addresses[i]);
		}
		g_free(m->config.proxy.backend_addresses);
	}

	if (m->config.proxy.address) {
		network_mysqld_proxy_free(NULL);

		g_free(m->config.proxy.address);
	}
	if (m->config.admin.address) {
		g_free(m->config.admin.address);
	}
#ifdef HAVE_EVENT_BASE_FREE
	/* only recent versions have this call */
	event_base_free(m->event_base);
#endif
	
	g_free(m);
}



/**
 * translate a address-string into a network_address structure
 *
 * - if the address contains a colon we assume IPv4, 
 *   - ":3306" -> (tcp) "0.0.0.0:3306"
 * - if it starts with a / it is a unix-domain socket 
 *   - "/tmp/socket" -> (unix) "/tmp/socket"
 *
 * @param addr     the address-struct
 * @param address  the address string
 * @return 0 on success, -1 otherwise
 */
int network_mysqld_con_set_address(network_address *addr, gchar *address) {
	gchar *s;
	guint port;

	/* split the address:port */
	if (NULL != (s = strchr(address, ':'))) {
		port = strtoul(s + 1, NULL, 10);

		if (port == 0) {
			g_critical("<ip>:<port>, port is invalid or 0, has to be > 0, got '%s'", address);
			return -1;
		}
		if (port > 65535) {
			g_critical("<ip>:<port>, port is too large, has to be < 65536, got '%s'", address);

			return -1;
		}

		memset(&addr->addr.ipv4, 0, sizeof(struct sockaddr_in));

		if (address == s || 
		    0 == strcmp("0.0.0.0", address)) {
			/* no ip */
			addr->addr.ipv4.sin_addr.s_addr = htonl(INADDR_ANY);
		} else {
			struct hostent *he;

			*s = '\0';
			he = gethostbyname(address);
			*s = ':';

			if (NULL == he)  {
				g_error("resolving proxy-address '%s' failed: ", address);
			}

			g_assert(he->h_addrtype == AF_INET);
			g_assert(he->h_length == sizeof(struct in_addr));

			memcpy(&(addr->addr.ipv4.sin_addr.s_addr), he->h_addr_list[0], he->h_length);
		}

		addr->addr.ipv4.sin_family = AF_INET;
		addr->addr.ipv4.sin_port = htons(port);
		addr->len = sizeof(struct sockaddr_in);

		addr->str = g_strdup(address);
#ifdef HAVE_SYS_UN_H
	} else if (address[0] == '/') {
		if (strlen(address) >= sizeof(addr->addr.un.sun_path) - 1) {
			g_critical("unix-path is too long: %s", address);
			return -1;
		}

		addr->addr.un.sun_family = AF_UNIX;
		strcpy(addr->addr.un.sun_path, address);
		addr->len = sizeof(struct sockaddr_un);
		addr->str = g_strdup(address);
#endif
	} else {
		/* might be a unix socket */
		g_critical("%s.%d: network_mysqld_con_set_address(%s) failed: address has to be <ip>:<port> for TCP or a absolute path starting with / for Unix sockets", 
				__FILE__, __LINE__,
				address);
		return -1;
	}

	return 0;
}

/**
 * connect a socket
 *
 * the con->addr has to be set before 
 * 
 * @param con    a socket 
 * @return       0 on connected, -1 on error, -2 for try again
 * @see network_mysqld_set_address()
 */
int network_mysqld_con_connect(network_socket * con) {
	int val = 1;

	g_assert(con->addr.len);

	/**
	 * con->addr.addr.ipv4.sin_family is always mapped to the same field 
	 * even if it is not a IPv4 address as we use a union
	 */
	if (-1 == (con->fd = socket(con->addr.addr.ipv4.sin_family, SOCK_STREAM, 0))) {
		g_critical("%s.%d: socket(%s) failed: %s", 
				__FILE__, __LINE__,
				con->addr.str, strerror(errno));
		return -1;
	}

	/**
	 * make the connect() call non-blocking
	 *
	 */
	network_mysqld_con_set_non_blocking(con->fd);

	if (-1 == connect(con->fd, (struct sockaddr *) &(con->addr.addr), con->addr.len)) {
		/**
		 * in most TCP cases we connect() will return with 
		 * EINPROGRESS ... 3-way handshake
		 */
		switch (errno) {
		case EINPROGRESS:
			return -2;
		default:
			g_critical("%s.%d: connect(%s) failed: %s", 
					__FILE__, __LINE__,
					con->addr.str,
					strerror(errno));
			return -1;
		}
	}

	/**
	 * set the same options as the mysql client 
	 */
	val = 8;
	setsockopt(con->fd, IPPROTO_IP,     IP_TOS, &val, sizeof(val));
	val = 1;
	setsockopt(con->fd, IPPROTO_TCP,    TCP_NODELAY, &val, sizeof(val) );
	val = 1;
	setsockopt(con->fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val) );

	return 0;
}


/**
 * connect a socket
 *
 * the con->addr has to be set before 
 * 
 * @param con    a socket 
 * @return       0 on connected, -1 on error, -2 for try again
 * @see network_mysqld_set_address()
 */
int network_mysqld_con_bind(network_socket * con) {
	int val = 1;

	g_assert(con->addr.len);

	if (-1 == (con->fd = socket(con->addr.addr.ipv4.sin_family, SOCK_STREAM, 0))) {
		g_critical("%s.%d: socket(%s) failed: %s", 
				__FILE__, __LINE__,
				con->addr.str, strerror(errno));
		return -1;
	}

	setsockopt(con->fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
	setsockopt(con->fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	if (-1 == bind(con->fd, (struct sockaddr *) &(con->addr.addr), con->addr.len)) {
		g_critical("%s.%d: bind(%s) failed: %s", 
				__FILE__, __LINE__,
				con->addr.str,
				strerror(errno));
		return -1;
	}

	if (-1 == listen(con->fd, 8)) {
		g_critical("%s.%d: listen() failed: %s",
				__FILE__, __LINE__,
				strerror(errno));
		return -1;
	}

	return 0;
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

/**
 * create a OK packet and append it to the send-queue
 *
 * @param con             a client socket 
 * @param affected_rows   affected rows 
 * @param insert_id       insert_id 
 * @param server_status   server_status (bitfield of SERVER_STATUS_*) 
 * @return 0
 *
 * @todo move to network_mysqld_proto
 */
int network_mysqld_con_send_ok_full(network_socket *con, guint64 affected_rows, guint64 insert_id, guint16 server_status, guint16 warnings ) {
	GString *packet = g_string_new(NULL);
	
	network_mysqld_proto_append_int8(packet, 0); /* no fields */
	network_mysqld_proto_append_lenenc_int(packet, affected_rows);
	network_mysqld_proto_append_lenenc_int(packet, insert_id);
	network_mysqld_proto_append_int16(packet, server_status); /* autocommit */
	network_mysqld_proto_append_int16(packet, warnings); /* no warnings */

	network_queue_append(con->send_queue, packet->str, packet->len, con->packet_id);

	g_string_free(packet, TRUE);

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
	
	packet = g_string_sized_new(10 + errmsg_len);
	
	network_mysqld_proto_append_int8(packet, 0xff); /* ERR */
	network_mysqld_proto_append_int16(packet, errorcode); /* errorcode */
	g_string_append_c(packet, '#');
	if (!sqlstate) {
		g_string_append_len(packet, C("07000"));
	} else {
		g_string_append_len(packet, sqlstate, 5);
	}

	if (errmsg_len < 512) {
		g_string_append_len(packet, errmsg, errmsg_len);
	} else {
		/* truncate the err-msg */
		g_string_append_len(packet, errmsg, 512);
	}

	network_queue_append(con->send_queue, packet->str, packet->len, con->packet_id);

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
 * read a data from the socket
 *
 */
retval_t network_mysqld_read_raw(network_mysqld *UNUSED_PARAM(srv), network_socket *con, GString *dest, size_t we_want) {
	gssize len;

	/**
	 * nothing to read, let's get out of here 
	 */
	if (we_want - dest->len == 0) {
		return RET_SUCCESS;
	}

	if (-1 == (len = recv(con->fd, dest->str + dest->len, we_want - dest->len, 0))) {
#ifdef _WIN32
		errno = WSAGetLastError();
#endif
		switch (errno) {
		case E_NET_CONNABORTED:
		case E_NET_CONNRESET: /** nothing to read, let's let ioctl() handle the close for us */
		case E_NET_WOULDBLOCK: /** the buffers are empty, try again later */
		case EAGAIN:     
			return RET_WAIT_FOR_EVENT;
		default:
			g_debug("%s: recv() failed: %s (errno=%d)", G_STRLOC, strerror(errno), errno);
			return RET_ERROR;
		}
	} else if (len == 0) {
		/**
		 * connection close
		 *
		 * let's call the ioctl() and let it handle it for use
		 */
		return RET_WAIT_FOR_EVENT;
	}

	dest->len += len;
	dest->str[dest->len] = '\0';

	if (dest->len < we_want) {
		/* we don't have enough */

		return RET_WAIT_FOR_EVENT;
	}

	return RET_SUCCESS;
}

/**
 * read a MySQL packet from the socket
 *
 * the packet is added to the con->recv_queue and contains a full mysql packet
 * with packet-header and everything 
 */
retval_t network_mysqld_read(network_mysqld *srv, network_socket *con) {
	GString *packet = NULL;

	/** 
	 * read the packet header (4 bytes)
	 */
	if (con->packet_len == PACKET_LEN_UNSET) {
		switch (network_mysqld_read_raw(srv, con, con->header, NET_HEADER_SIZE)) {
		case RET_WAIT_FOR_EVENT:
			return RET_WAIT_FOR_EVENT;
		case RET_ERROR:
			return RET_ERROR;
		case RET_SUCCESS:
			break;
		case RET_ERROR_RETRY:
			g_error("RET_ERROR_RETRY wasn't expected");
			break;
		}

		con->packet_len = network_mysqld_proto_get_header((unsigned char *)(con->header->str));
		con->packet_id  = (unsigned char)(con->header->str[3]); /* packet-id if the next packet */

		packet = g_string_sized_new(con->packet_len + NET_HEADER_SIZE + 1); /* we need some space for the \0 */
		g_string_append_len(packet, con->header->str, NET_HEADER_SIZE); /* copy the header */

		network_queue_append_chunk(con->recv_queue, packet);
		g_string_truncate(con->header, 0);
	} else {
		packet = con->recv_queue->chunks->tail->data;
	}

	g_assert(packet->allocated_len >= con->packet_len + NET_HEADER_SIZE);

	switch (network_mysqld_read_raw(srv, con, packet, con->packet_len + NET_HEADER_SIZE)) {
	case RET_WAIT_FOR_EVENT:
		return RET_WAIT_FOR_EVENT;
	case RET_ERROR:
		return RET_ERROR;
	case RET_SUCCESS:
		break;
	case RET_ERROR_RETRY:
		g_error("RET_ERROR_RETRY wasn't expected");
		break;

	}

	return RET_SUCCESS;
}

/**
 * write data to the socket
 *
 */
retval_t network_mysqld_write_len(network_mysqld *UNUSED_PARAM(srv), network_socket *con, int send_chunks) {
	/* send the whole queue */
	GList *chunk;

	if (send_chunks == 0) return RET_SUCCESS;

	for (chunk = con->send_queue->chunks->head; chunk; ) {
		GString *s = chunk->data;
		gssize len;

		g_assert(con->send_queue->offset < s->len);

		if (-1 == (len = send(con->fd, s->str + con->send_queue->offset, s->len - con->send_queue->offset, 0))) {
#ifdef _WIN32
			errno = WSAGetLastError();
#endif
			switch (errno) {
			case E_NET_WOULDBLOCK:
			case EAGAIN:
				return RET_WAIT_FOR_EVENT;
			case EPIPE:
			case E_NET_CONNRESET:
			case E_NET_CONNABORTED:
				/** remote side closed the connection */
				return RET_ERROR;
			default:
				g_message("%s.%d: send(%s, %ld) failed: %s", 
						__FILE__, __LINE__, 
						con->addr.str, 
						s->len - con->send_queue->offset, 
						strerror(errno));
				return RET_ERROR;
			}
		} else if (len == 0) {
			return RET_ERROR;
		}

		con->send_queue->offset += len;

		if (con->send_queue->offset == s->len) {
			g_string_free(s, TRUE);
			
			g_queue_delete_link(con->send_queue->chunks, chunk);
			con->send_queue->offset = 0;

			if (send_chunks > 0 && --send_chunks == 0) break;

			chunk = con->send_queue->chunks->head;
		} else {
			return RET_WAIT_FOR_EVENT;
		}
	}

	return RET_SUCCESS;
}

retval_t network_mysqld_write(network_mysqld *srv, network_socket *con) {
	retval_t ret;

	ret = network_mysqld_write_len(srv, con, -1);

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
 * @return         RET_SUCCESS on success
 */
retval_t plugin_call(network_mysqld *srv, network_mysqld_con *con, int state) {
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
			switch (con->parse.state.auth_result.state) {
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
						con->parse.state.auth_result.state);
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
		if (packet->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS;

		network_queue_append_chunk(send_sock->send_queue, packet);

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
	default:
		g_error("%s.%d: unhandled state: %d", 
				__FILE__, __LINE__,
				state);
	}
	if (!func) return RET_SUCCESS;

	return (*func)(srv, con);
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
	network_mysqld *srv = con->srv;

	g_assert(srv);
	g_assert(con);

	if (events == EV_READ) {
		int b = -1;

		if (ioctl(event_fd, FIONREAD, &b)) {
			g_critical("ioctl(%d, FIONREAD, ...) failed: %s", event_fd, strerror(errno));

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
		switch (con->state) {
		case CON_STATE_ERROR:
			/* we can't go on, close the connection */
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
			case RET_SUCCESS:
				break;
			default:
				/**
				 * no luck, let's close the connection
				 */
				g_critical("%s.%d: plugin_call(CON_STATE_INIT) != RET_SUCCESS", __FILE__, __LINE__);

				con->state = CON_STATE_ERROR;
				
				break;
			}

			break;
		case CON_STATE_CONNECT_SERVER:
			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:

				/**
				 * hmm, if this is success and we have something in the clients send-queue
				 * we just send it out ... who needs a server ? */

				if (con->client->send_queue->chunks->length > 0 && con->server == NULL) {
					/* we want to send something to the client */

					con->state = CON_STATE_SEND_HANDSHAKE;
				} else {
					g_assert(con->server);
				}

				break;
			case RET_ERROR_RETRY:
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
			case RET_ERROR:
				/**
				 * connecting failed and no option to retry
				 *
				 * close the connection
				 */
				con->state = CON_STATE_SEND_ERROR;
				break;
			default:
				g_error("%s.%d: ...", __FILE__, __LINE__);
				
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
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				/* call us again when you have a event */
				WAIT_FOR_EVENT(con->server, EV_READ, NULL);

				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				g_error("%s.%d: plugin_call(CON_STATE_CONNECT_SERVER) returned an error", __FILE__, __LINE__);
				break;
			}

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			case RET_ERROR:
				/**
				 * we couldn't understand the pack from the server 
				 * 
				 * we have something in the queue and will send it to the client
				 * and close the connection afterwards
				 */
				
				con->state = CON_STATE_SEND_ERROR;

				break;
			default:
				g_error("%s.%d: ...", __FILE__, __LINE__);
				break;
			}
	
			break; }
		case CON_STATE_SEND_HANDSHAKE: 
			/* send the hand-shake to the client and wait for a response */
			
			switch (network_mysqld_write(srv, con->client)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_WRITE, NULL);
				
				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				/**
				 * writing failed, closing connection
				 */
				con->state = CON_STATE_ERROR;
				break;
			}

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			default:
				g_error("%s.%d: plugin_call(CON_STATE_SEND_HANDSHAKE) != RET_SUCCESS", __FILE__, __LINE__);
				break;
			}

			break;
		case CON_STATE_READ_AUTH: {
			/* read auth from client */
			network_socket *recv_sock;

			recv_sock = con->client;

			g_assert(events == 0 || event_fd == recv_sock->fd);

			switch (network_mysqld_read(srv, recv_sock)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_READ, NULL);

				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				g_error("%s.%d: network_mysqld_read(CON_STATE_READ_AUTH) returned an error", __FILE__, __LINE__);
				return;
			}

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			default:
				g_error("%s.%d: plugin_call(CON_STATE_READ_AUTH) != RET_SUCCESS", __FILE__, __LINE__);
				break;
			}

			break; }
		case CON_STATE_SEND_AUTH:
			/* send the auth-response to the server */
			switch (network_mysqld_write(srv, con->server)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->server, EV_WRITE, NULL);

				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				/* might be a connection close, we should just close the connection and be happy */
				con->state = CON_STATE_ERROR;

				return;
			}

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			default:
				g_error("%s.%d: plugin_call(CON_STATE_SEND_AUTH) != RET_SUCCESS", __FILE__, __LINE__);
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
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->server, EV_READ, NULL);
				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				g_error("%s.%d: network_mysqld_read(CON_STATE_READ_AUTH_RESULT) returned an error", __FILE__, __LINE__);
				return;
			}

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
			con->parse.state.auth_result.state = packet->str[NET_HEADER_SIZE];

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			default:
				g_critical("%s.%d: plugin_call(CON_STATE_READ_AUTH_RESULT) != RET_SUCCESS", __FILE__, __LINE__);

				con->state = CON_STATE_ERROR;
				break;
			}

			break; }
		case CON_STATE_SEND_AUTH_RESULT: {
			/* send the hand-shake to the client and wait for a response */

			switch (network_mysqld_write(srv, con->client)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_WRITE, NULL);
				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				g_error("%s.%d: network_mysqld_write(CON_STATE_SEND_AUTH_RESULT) returned an error", __FILE__, __LINE__);
				return;
			}

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			default:
				g_error("%s.%d: ...", __FILE__, __LINE__);
				break;
			}
				
			break; }
		case CON_STATE_READ_AUTH_OLD_PASSWORD: 
			/* read auth from client */

			switch (network_mysqld_read(srv, con->client)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_READ, NULL);

				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				g_error("%s.%d: network_mysqld_read(CON_STATE_READ_AUTH_OLD_PASSWORD) returned an error", __FILE__, __LINE__);
				return;
			}

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			default:
				g_error("%s.%d: plugin_call(CON_STATE_READ_AUTH_OLD_PASSWORD) != RET_SUCCESS", __FILE__, __LINE__);
				break;
			}

			break; 
		case CON_STATE_SEND_AUTH_OLD_PASSWORD:
			/* send the auth-response to the server */
			switch (network_mysqld_write(srv, con->server)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->server, EV_WRITE, NULL);

				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				/* might be a connection close, we should just close the connection and be happy */
				g_error("%s.%d: network_mysqld_write(CON_STATE_SEND_AUTH_OLD_PASSWORD) returned an error", __FILE__, __LINE__);
				return;
			}

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			default:
				g_error("%s.%d: plugin_call(CON_STATE_SEND_AUTH_OLD_PASSWORD) != RET_SUCCESS", __FILE__, __LINE__);
				break;
			}

			break;

		case CON_STATE_READ_QUERY: {
			network_socket *recv_sock;

			recv_sock = con->client;

			g_assert(events == 0 || event_fd == recv_sock->fd);

			switch (network_mysqld_read(srv, recv_sock)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_READ, NULL);
				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				g_error("%s.%d: network_mysqld_read(CON_STATE_READ_QUERY) returned an error", __FILE__, __LINE__);
				return;
			}

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			default:
				g_critical("%s.%d: plugin_call(CON_STATE_READ_QUERY) failed", __FILE__, __LINE__);

				con->state = CON_STATE_ERROR;
				break;
			}

			break; }
		case CON_STATE_SEND_QUERY: 
			/* send the query to the server */

			if (con->server->send_queue->offset == 0) {
				/* only parse the packets once */
				GString *s;
				GList *chunk;

				chunk = con->server->send_queue->chunks->head;
				s = chunk->data;

				/* only parse once and don't care about the blocking read */
				if (con->parse.command == COM_QUERY &&
				    con->parse.state.query == PARSE_COM_QUERY_LOAD_DATA) {
					/* is this a LOAD DATA INFILE ... extra round ? */
					/* this isn't a command packet, but a LOAD DATA INFILE data-packet */
					if (s->str[0] == 0 && s->str[1] == 0 && s->str[2] == 0) {
						con->parse.state.query = PARSE_COM_QUERY_LOAD_DATA_END_DATA;
					}
				} else if (con->is_overlong_packet) {
					/* the last packet was a over-long packet
					 * this is the same command, just more data */
	
					if (con->parse.len != PACKET_LEN_MAX) {
						con->is_overlong_packet = 0;
					}
	
				} else {
					con->parse.command = s->str[4];
	
					if (con->parse.len == PACKET_LEN_MAX) {
						con->is_overlong_packet = 1;
					}
		
					/* init the parser for the commands */
					switch (con->parse.command) {
					case COM_QUERY:
					case COM_STMT_EXECUTE:
						con->parse.state.query = PARSE_COM_QUERY_INIT;
						break;
					case COM_STMT_PREPARE:
						con->parse.state.prepare.first_packet = 1;
						break;
					case COM_INIT_DB:
						if (s->str[NET_HEADER_SIZE] == COM_INIT_DB && 
						    (s->len > NET_HEADER_SIZE + 1)) {
							con->parse.state.init_db.db_name = g_string_new(NULL);
				
							g_string_truncate(con->parse.state.init_db.db_name, 0);
							g_string_append_len(con->parse.state.init_db.db_name, 
									s->str + NET_HEADER_SIZE + 1, 
									s->len - NET_HEADER_SIZE - 1);
						} else {
							con->parse.state.init_db.db_name = NULL;
						}

						break;
					default:
						break;
					}
				}
			}
	
			switch (network_mysqld_write_len(srv, con->server, 1)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->server, EV_WRITE, NULL);
				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				g_error("%s.%d: network_mysqld_write(CON_STATE_SEND_QUERY) returned an error", __FILE__, __LINE__);
				return;
			}

			if (con->is_overlong_packet) {
				con->state = CON_STATE_READ_QUERY;
				break;
			}

			/* some statements don't have a server response */
			switch (con->parse.command) {
			case COM_STMT_SEND_LONG_DATA: /* not acked */
			case COM_STMT_CLOSE:
				con->state = CON_STATE_READ_QUERY;
				break;
			case COM_QUERY:
				if (con->parse.state.query == PARSE_COM_QUERY_LOAD_DATA) {
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
			do {
				network_socket *recv_sock;

				recv_sock = con->server;

				g_assert(events == 0 || event_fd == recv_sock->fd);

				switch (network_mysqld_read(srv, recv_sock)) {
				case RET_SUCCESS:
					break;
				case RET_WAIT_FOR_EVENT:
					WAIT_FOR_EVENT(con->server, EV_READ, NULL);
					return;
				case RET_ERROR_RETRY:
				case RET_ERROR:
					g_error("%s.%d: network_mysqld_read(CON_STATE_READ_QUERY_RESULT) returned an error", __FILE__, __LINE__);
					return;
				}

				switch (plugin_call(srv, con, con->state)) {
				case RET_SUCCESS:
					break;
				default:
					g_error("%s.%d: ...", __FILE__, __LINE__);
					break;
				}

			} while (con->state == CON_STATE_READ_QUERY_RESULT);

			if (con->parse.command == COM_INIT_DB) {
				if (con->parse.state.init_db.db_name) {
					g_string_free(con->parse.state.init_db.db_name, TRUE);
					con->parse.state.init_db.db_name = NULL;
				}
			}
	
			break; 
		case CON_STATE_SEND_QUERY_RESULT:
			/**
			 * send the query result-set to the client */

			switch (network_mysqld_write(srv, con->client)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_WRITE, NULL);
				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
				/**
				 * client is gone away
				 *
				 * close the connection and clean up
				 */
				con->state = CON_STATE_ERROR;
				break;
			}

			/* if the write failed, don't call the plugin handlers */
			if (con->state != CON_STATE_SEND_QUERY_RESULT) break;

			switch (plugin_call(srv, con, con->state)) {
			case RET_SUCCESS:
				break;
			default:
				g_error("%s.%d: ...", __FILE__, __LINE__);
				break;
			}

			break;
		case CON_STATE_SEND_ERROR:
			/**
			 * send error to the client
			 * and close the connections afterwards
			 *  */

			switch (network_mysqld_write(srv, con->client)) {
			case RET_SUCCESS:
				break;
			case RET_WAIT_FOR_EVENT:
				WAIT_FOR_EVENT(con->client, EV_WRITE, NULL);
				return;
			case RET_ERROR_RETRY:
			case RET_ERROR:
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
 * portable 'set non-blocking io'
 *
 * @param fd      socket-fd
 * @return        0
 */
int network_mysqld_con_set_non_blocking(int fd) {
	int ioctlvar;

#ifdef _WIN32
	ioctlvar = 1;
	ioctlsocket(fd, FIONBIO, &ioctlvar);
#else
	fcntl(fd, F_SETFL, O_NONBLOCK | O_RDWR);
#endif
	return 0;
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
	network_mysqld_con *con = user_data;
	network_mysqld_con *client_con;
	socklen_t addr_len;
	struct sockaddr_in ipv4;
	int fd;

	g_assert(events == EV_READ);
	g_assert(con->server);

	addr_len = sizeof(struct sockaddr_in);

	if (-1 == (fd = accept(event_fd, (struct sockaddr *)&ipv4, &addr_len))) {
		return ;
	}

	network_mysqld_con_set_non_blocking(fd);

	/* looks like we open a client connection */
	client_con = network_mysqld_con_init(con->srv);
	client_con->client = network_socket_init();
	client_con->client->addr.addr.ipv4 = ipv4;
	client_con->client->addr.len = addr_len;
	client_con->client->fd   = fd;

	/* resolve the peer-addr if necessary */
	if (!client_con->client->addr.str) {
		switch (client_con->client->addr.addr.common.sa_family) {
		case AF_INET:
			client_con->client->addr.str = g_strdup_printf("%s:%d", 
					inet_ntoa(client_con->client->addr.addr.ipv4.sin_addr),
					client_con->client->addr.addr.ipv4.sin_port);
			break;
		default:
			g_message("%s.%d: can't convert addr-type %d into a string", 
					 __FILE__, __LINE__, 
					 client_con->client->addr.addr.common.sa_family);
			break;
		}
	}


	/* copy the config
	 *
	 * @todo replace network-type by a function pointer for the init 
	 *
	 */
	client_con->config = con->config;
	client_con->config.network_type = con->config.network_type;

	switch (con->config.network_type) {
	case NETWORK_TYPE_SERVER:
		network_mysqld_server_connection_init(client_con);
		break;
	case NETWORK_TYPE_PROXY:
		network_mysqld_proxy_connection_init(client_con);
		break;
	default:
		g_error("%s.%d", __FILE__, __LINE__);
		break;
	}

	network_mysqld_con_handle(-1, 0, client_con);

	return;
}


/**
 * timeout handler for the event-loop 
 */
static void handle_timeout() {
	if (!agent_shutdown) return;

	/* we have to shutdown, disable all events to leave the dispatch */
}

void *network_mysqld_thread(void *_srv) {
	network_mysqld *srv = _srv;
	network_mysqld_con *proxy_con = NULL, *admin_con = NULL;

#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD( 2, 2 );

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		return NULL;
	}
#endif

	/* setup the different handlers */

	if (srv->config.admin.address) {
		network_mysqld_con *con = NULL;

		con = network_mysqld_con_init(srv);
		con->config = srv->config;
		con->config.network_type = NETWORK_TYPE_SERVER;
		
		con->server = network_socket_init();

		if (0 != network_mysqld_server_init(con)) {
			g_critical("%s.%d: network_mysqld_server_init() failed", __FILE__, __LINE__);

			return NULL;
		}

		/* keep the listen socket alive */
		event_set(&(con->server->event), con->server->fd, EV_READ|EV_PERSIST, network_mysqld_con_accept, con);
		event_base_set(srv->event_base, &(con->server->event));
		event_add(&(con->server->event), NULL);
		
		admin_con = con;
	}

	if (srv->config.proxy.address) {
		network_mysqld_con *con = NULL;

		con = network_mysqld_con_init(srv);
		con->config = srv->config;
		con->config.network_type = NETWORK_TYPE_PROXY;
		
		con->server = network_socket_init();

		if (0 != network_mysqld_proxy_init(con)) {
			g_critical("%s.%d: network_mysqld_server_init() failed", __FILE__, __LINE__);
			return NULL;
		}
	
		/* keep the listen socket alive */
		event_set(&(con->server->event), con->server->fd, EV_READ|EV_PERSIST, network_mysqld_con_accept, con);
		event_base_set(srv->event_base, &(con->server->event));
		event_add(&(con->server->event), NULL);

		proxy_con = con;
	}

	/**
	 * check once a second if we shall shutdown the proxy
	 */
	while (!agent_shutdown) {
		struct timeval timeout;
		int r;

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		g_assert(event_base_loopexit(srv->event_base, &timeout) == 0);

		r = event_base_dispatch(srv->event_base);

		if (r == -1) {
			if (errno == EINTR) continue;

			break;
		}
	}

	/**
	 * cleanup
	 *
	 */
	if (proxy_con) {
		/**
		 * we still might have connections pointing to the close scope */
		event_del(&(proxy_con->server->event));
		network_mysqld_con_free(proxy_con);
	}
	
	if (admin_con) {
		event_del(&(admin_con->server->event));
		network_mysqld_con_free(admin_con);
	}

	return NULL;
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
	network_queue_append(con->send_queue, s->str, s->len, con->packet_id++);

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
		network_queue_append(con->send_queue, s->str, s->len, con->packet_id++);
	}

	g_string_truncate(s, 0);
	
	/* EOF */	
	g_string_append_len(s, "\xfe", 1); /* EOF */
	g_string_append_len(s, "\x00\x00", 2); /* warning count */
	g_string_append_len(s, "\x02\x00", 2); /* flags */
	
	network_queue_append(con->send_queue, s->str, s->len, con->packet_id++);

	for (i = 0; i < rows->len; i++) {
		GPtrArray *row = rows->pdata[i];

		g_string_truncate(s, 0);

		for (j = 0; j < row->len; j++) {
			network_mysqld_proto_append_lenenc_string(s, row->pdata[j]);
		}
		network_queue_append(con->send_queue, s->str, s->len, con->packet_id++);
	}

	g_string_truncate(s, 0);

	/* EOF */	
	g_string_append_len(s, "\xfe", 1); /* EOF */
	g_string_append_len(s, "\x00\x00", 2); /* warning count */
	g_string_append_len(s, "\x02\x00", 2); /* flags */

	network_queue_append(con->send_queue, s->str, s->len, con->packet_id++);

	g_string_free(s, TRUE);

	return 0;
}


