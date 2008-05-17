/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifdef HAVE_CONFIG_H
#include "config.h"
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
#include <errno.h>
#include <fcntl.h>

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


#include "network-socket.h"
#include "network-mysqld-proto.h"

network_queue *network_queue_init() {
	network_queue *queue;

	queue = g_new0(network_queue, 1);

	queue->chunks = g_queue_new();
	
	return queue;
}

void network_queue_free(network_queue *queue) {
	GString *packet;

	if (!queue) return;

	while ((packet = g_queue_pop_head(queue->chunks))) g_string_free(packet, TRUE);

	g_queue_free(queue->chunks);

	g_free(queue);
}

int network_queue_append(network_queue *queue, GString *s) {
	queue->len += s->len;

	g_queue_push_tail(queue->chunks, s);

	return 0;
}

/**
 * get a string from the head of the queue and leave the queue unchanged 
 *
 * @param  peek_len bytes to collect
 * @param  dest 
 * @return NULL if not enough data
 *         if dest is not NULL, dest, otherwise a new GString containing the data
 */
GString *network_queue_peek_string(network_queue *queue, gsize peek_len, GString *dest) {
	gsize we_want = peek_len;
	GList *node;

	if (queue->len < peek_len) {
		return NULL;
	}

	if (!dest) {
		/* no define */
		dest = g_string_sized_new(peek_len);
	}

	g_assert_cmpint(dest->allocated_len, >, peek_len);

	for (node = queue->chunks->head; node && we_want; node = node->next) {
		GString *chunk = node->data;

		if (node == queue->chunks->head) {
			gsize we_have = we_want < (chunk->len - queue->offset) ? we_want : (chunk->len - queue->offset);

			g_string_append_len(dest, chunk->str + queue->offset, we_have);
			
			we_want -= we_have;
		} else {
			gsize we_have = we_want < chunk->len ? we_want : chunk->len;
			
			g_string_append_len(dest, chunk->str, we_have);

			we_want -= we_have;
		}
	}

	return dest;
}

/**
 * get a string from the head of the queue and remove the chunks from the queue 
 */
GString *network_queue_pop_string(network_queue *queue, gsize steal_len, GString *dest) {
	gsize we_want = steal_len;
	GString *chunk;

	if (queue->len < steal_len) {
		return NULL;
	}

	if (!dest) {
		dest = g_string_sized_new(steal_len);
	}
	
	g_assert_cmpint(dest->allocated_len, >, steal_len);

	while ((chunk = g_queue_peek_head(queue->chunks))) {
		gsize we_have = we_want < (chunk->len - queue->offset) ? we_want : (chunk->len - queue->offset);

		g_string_append_len(dest, chunk->str + queue->offset, we_have);

		queue->offset += we_have;
		queue->len    -= we_have;
		we_want -= we_have;

		if (chunk->len == queue->offset) {
			/* the chunk is done, remove it */
			g_string_free(g_queue_pop_head(queue->chunks), TRUE);
			queue->offset = 0;
		} else {
			break;
		}
	}

	return dest;
}


network_socket *network_socket_init() {
	network_socket *s;
	
	s = g_new0(network_socket, 1);

	s->send_queue = network_queue_init();
	s->recv_queue = network_queue_init();
	s->recv_queue_raw = network_queue_init();

	s->packet_len = PACKET_LEN_UNSET;

	s->default_db = g_string_new(NULL);
	s->username   = g_string_new(NULL);
	s->scrambled_password = g_string_new(NULL);
	s->scramble_buf = g_string_new(NULL);
	s->auth_handshake_packet = g_string_new(NULL);
	s->fd           = -1;

	return s;
}

void network_socket_free(network_socket *s) {
	if (!s) return;

	network_queue_free(s->send_queue);
	network_queue_free(s->recv_queue);
	network_queue_free(s->recv_queue_raw);

	if (s->addr.str) {
		g_free(s->addr.str);
	}

	event_del(&(s->event));

	if (s->fd != -1) {
		closesocket(s->fd);
	}

	g_string_free(s->scramble_buf, TRUE);
	g_string_free(s->auth_handshake_packet, TRUE);
	g_string_free(s->username,   TRUE);
	g_string_free(s->default_db, TRUE);
	g_string_free(s->scrambled_password, TRUE);

	g_free(s);
}

/**
 * portable 'set non-blocking io'
 *
 * @param fd      socket-fd
 * @return        0
 */
network_socket_retval_t network_socket_set_non_blocking(network_socket *sock) {
	int ret;
#ifdef _WIN32
	int ioctlvar;

	ioctlvar = 1;
	ret = ioctlsocket(sock->fd, FIONBIO, &ioctlvar);
#else
	ret = fcntl(sock->fd, F_SETFL, O_NONBLOCK | O_RDWR);
#endif
	if (ret != 0) {
#ifdef _WIN32
		errno = WSAGetLastError();
#endif
		g_critical("%s.%d: set_non_blocking() failed: %s (%d)", 
				__FILE__, __LINE__,
				strerror(errno), errno);
		return NETWORK_SOCKET_ERROR;
	}
	return NETWORK_SOCKET_SUCCESS;
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
network_socket_retval_t network_socket_connect(network_socket * con) {
	int val = 1;

	g_assert(con->addr.len);

	/**
	 * con->addr.addr.ipv4.sin_family is always mapped to the same field 
	 * even if it is not a IPv4 address as we use a union
	 */
	if (-1 == (con->fd = socket(con->addr.addr.ipv4.sin_family, SOCK_STREAM, 0))) {
#ifdef _WIN32
		errno = WSAGetLastError();
#endif
		g_critical("%s.%d: socket(%s) failed: %s (%d)", 
				__FILE__, __LINE__,
				con->addr.str, strerror(errno), errno);
		return -1;
	}

	/**
	 * make the connect() call non-blocking
	 *
	 */
	network_socket_set_non_blocking(con);

	if (-1 == connect(con->fd, (struct sockaddr *) &(con->addr.addr), con->addr.len)) {
#ifdef _WIN32
		errno = WSAGetLastError();
#endif
		/**
		 * in most TCP cases we connect() will return with 
		 * EINPROGRESS ... 3-way handshake
		 */
		switch (errno) {
		case E_NET_INPROGRESS:
		case E_NET_WOULDBLOCK: /* win32 uses WSAEWOULDBLOCK */
			return -2;
		default:
			g_critical("%s.%d: connect(%s) failed: %s (%d)", 
					__FILE__, __LINE__,
					con->addr.str,
					strerror(errno), errno);
			return -1;
		}
	}

	/**
	 * set the same options as the mysql client 
	 */
#ifdef IP_TOS
	val = 8;
	setsockopt(con->fd, IPPROTO_IP,     IP_TOS, &val, sizeof(val));
#endif
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
network_socket_retval_t network_socket_bind(network_socket * con) {
	int val = 1;

	g_assert(con->addr.len);

	if (-1 == (con->fd = socket(con->addr.addr.ipv4.sin_family, SOCK_STREAM, 0))) {
		g_critical("%s.%d: socket(%s) failed: %s", 
				__FILE__, __LINE__,
				con->addr.str, strerror(errno));
		return NETWORK_SOCKET_ERROR;
	}

	setsockopt(con->fd, IPPROTO_TCP, TCP_NODELAY, &val, sizeof(val));
	setsockopt(con->fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

	if (-1 == bind(con->fd, (struct sockaddr *) &(con->addr.addr), con->addr.len)) {
		g_critical("%s.%d: bind(%s) failed: %s", 
				__FILE__, __LINE__,
				con->addr.str,
				strerror(errno));
		return NETWORK_SOCKET_ERROR;
	}

	if (-1 == listen(con->fd, 8)) {
		g_critical("%s.%d: listen() failed: %s",
				__FILE__, __LINE__,
				strerror(errno));
		return NETWORK_SOCKET_ERROR;
	}

	return NETWORK_SOCKET_SUCCESS;
}

/**
 * read a data from the socket
 *
 * @param sock the socket
 */
network_socket_retval_t network_socket_read(network_socket *sock) {
	gssize len;

	if (sock->to_read > 0) {
		GString *packet = g_string_sized_new(sock->to_read);

		g_queue_push_tail(sock->recv_queue_raw->chunks, packet);

		if (-1 == (len = recv(sock->fd, packet->str, sock->to_read, 0))) {
#ifdef _WIN32
			errno = WSAGetLastError();
#endif
			switch (errno) {
			case E_NET_CONNABORTED:
			case E_NET_CONNRESET: /** nothing to read, let's let ioctl() handle the close for us */
			case E_NET_WOULDBLOCK: /** the buffers are empty, try again later */
			case EAGAIN:     
				return NETWORK_SOCKET_WAIT_FOR_EVENT;
			default:
				g_debug("%s: recv() failed: %s (errno=%d)", G_STRLOC, strerror(errno), errno);
				return NETWORK_SOCKET_ERROR;
			}
		} else if (len == 0) {
			/**
			 * connection close
			 *
			 * let's call the ioctl() and let it handle it for use
			 */
			return NETWORK_SOCKET_WAIT_FOR_EVENT;
		}

		sock->to_read -= len;
		sock->recv_queue_raw->len += len;
#if 0
		sock->recv_queue_raw->offset = 0; /* offset into the first packet */
#endif
		packet->len = len;
	}

	return NETWORK_SOCKET_SUCCESS;
}

#ifdef HAVE_WRITEV
/**
 * write data to the socket
 *
 */
static network_socket_retval_t network_socket_write_writev(network_socket *con, int send_chunks) {
	/* send the whole queue */
	GList *chunk;
	struct iovec *iov;
	gint chunk_id;
	gint chunk_count;
	gssize len;
	int os_errno;

	if (send_chunks == 0) return NETWORK_SOCKET_SUCCESS;

	chunk_count = send_chunks > 0 ? send_chunks : (gint)con->send_queue->chunks->length;
	
	if (chunk_count == 0) return NETWORK_SOCKET_SUCCESS;

	chunk_count = chunk_count > sysconf(_SC_IOV_MAX) ? sysconf(_SC_IOV_MAX) : chunk_count;

	iov = g_new0(struct iovec, chunk_count);

	for (chunk = con->send_queue->chunks->head, chunk_id = 0; 
	     chunk && chunk_id < chunk_count; 
	     chunk_id++, chunk = chunk->next) {
		GString *s = chunk->data;
	
		if (chunk_id == 0) {
			g_assert(con->send_queue->offset < s->len);

			iov[chunk_id].iov_base = s->str + con->send_queue->offset;
			iov[chunk_id].iov_len  = s->len - con->send_queue->offset;
		} else {
			iov[chunk_id].iov_base = s->str;
			iov[chunk_id].iov_len  = s->len;
		}
	}

	len = writev(con->fd, iov, chunk_count);
	os_errno = errno;

	g_free(iov);

	if (-1 == len) {
		switch (os_errno) {
		case E_NET_WOULDBLOCK:
		case EAGAIN:
			return NETWORK_SOCKET_WAIT_FOR_EVENT;
		case EPIPE:
		case E_NET_CONNRESET:
		case E_NET_CONNABORTED:
			/** remote side closed the connection */
			return NETWORK_SOCKET_ERROR;
		default:
			g_message("%s.%d: writev(%s, ...) failed: %s", 
					__FILE__, __LINE__, 
					con->addr.str, 
					strerror(errno));
			return NETWORK_SOCKET_ERROR;
		}
	} else if (len == 0) {
		return NETWORK_SOCKET_ERROR;
	}

	con->send_queue->offset += len;

	/* check all the chunks which we have sent out */
	for (chunk = con->send_queue->chunks->head; chunk; ) {
		GString *s = chunk->data;

		if (con->send_queue->offset >= s->len) {
			con->send_queue->offset -= s->len;

			g_string_free(s, TRUE);
			
			g_queue_delete_link(con->send_queue->chunks, chunk);

			chunk = con->send_queue->chunks->head;
		} else {
			return NETWORK_SOCKET_WAIT_FOR_EVENT;
		}
	}

	return NETWORK_SOCKET_SUCCESS;
}
#endif

/**
 * write data to the socket
 *
 * use a loop over send() to be compatible with win32
 */
static network_socket_retval_t network_socket_write_send(network_socket *con, int send_chunks) {
	/* send the whole queue */
	GList *chunk;

	if (send_chunks == 0) return NETWORK_SOCKET_SUCCESS;

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
				return NETWORK_SOCKET_WAIT_FOR_EVENT;
			case EPIPE:
			case E_NET_CONNRESET:
			case E_NET_CONNABORTED:
				/** remote side closed the connection */
				return NETWORK_SOCKET_ERROR;
			default:
				g_message("%s: send(%s, %"G_GSIZE_FORMAT") failed: %s", 
						G_STRLOC, 
						con->addr.str, 
						s->len - con->send_queue->offset, 
						strerror(errno));
				return NETWORK_SOCKET_ERROR;
			}
		} else if (len == 0) {
			return NETWORK_SOCKET_ERROR;
		}

		con->send_queue->offset += len;

		if (con->send_queue->offset == s->len) {
			g_string_free(s, TRUE);
			
			g_queue_delete_link(con->send_queue->chunks, chunk);
			con->send_queue->offset = 0;

			if (send_chunks > 0 && --send_chunks == 0) break;

			chunk = con->send_queue->chunks->head;
		} else {
			return NETWORK_SOCKET_WAIT_FOR_EVENT;
		}
	}

	return NETWORK_SOCKET_SUCCESS;
}

network_socket_retval_t network_socket_write(network_socket *con, int send_chunks) {
#ifdef HAVE_WRITEV
	return network_socket_write_writev(con, send_chunks);
#else
	return network_socket_write_send(con, send_chunks);
#endif
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
network_socket_retval_t network_address_set_address(network_address *addr, gchar *address) {
	gchar *s;
	guint port;

	/* split the address:port */
	if (NULL != (s = strchr(address, ':'))) {
		port = strtoul(s + 1, NULL, 10);

		if (port == 0) {
			g_critical("<ip>:<port>, port is invalid or 0, has to be > 0, got '%s'", address);
			return NETWORK_SOCKET_ERROR;
		}
		if (port > 65535) {
			g_critical("<ip>:<port>, port is too large, has to be < 65536, got '%s'", address);

			return NETWORK_SOCKET_ERROR;
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
			return NETWORK_SOCKET_ERROR;
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
		return NETWORK_SOCKET_ERROR;
	}

	return NETWORK_SOCKET_SUCCESS;
}


network_socket_retval_t network_address_resolve_address(network_address *addr) {
	/* resolve the peer-addr if necessary */
	if (addr->len > 0) return NETWORK_SOCKET_SUCCESS;

	switch (addr->addr.common.sa_family) {
	case AF_INET:
		addr->str = g_strdup_printf("%s:%d", 
				inet_ntoa(addr->addr.ipv4.sin_addr),
				addr->addr.ipv4.sin_port);
		break;
	default:
		g_critical("%s.%d: can't convert addr-type %d into a string", 
				 __FILE__, __LINE__, 
				 addr->addr.common.sa_family);
		return NETWORK_SOCKET_ERROR;
	}

	return NETWORK_SOCKET_SUCCESS;

}
