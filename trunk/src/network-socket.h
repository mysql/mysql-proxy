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

#ifndef _NETWORK_SOCKET_H_
#define _NETWORK_SOCKET_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
/**
 * event.h needs struct timeval and doesn't include sys/time.h itself
 */
#include <sys/time.h>
#endif

#include <sys/types.h>      /** u_char */
#ifndef _WIN32
#include <sys/socket.h>     /** struct sockaddr */

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>     /** struct sockaddr_in */
#endif
#include <netinet/tcp.h>

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>         /** struct sockaddr_un */
#endif
/**
 * use closesocket() to close sockets to be compatible with win32
 */
#define closesocket(x) close(x)
#else
#include <winsock2.h>

#define socklen_t int
#endif
#include <glib.h>
#include <event.h>

/* a input or output stream */
typedef struct {
	GQueue *chunks;

	size_t len; /* len in all chunks */
	size_t offset; /* offset over all chunks */
} network_queue;

typedef struct {
	union {
		struct sockaddr_in ipv4;
#ifdef HAVE_SYS_UN_H
		struct sockaddr_un un;
#endif
		struct sockaddr common;
	} addr;

	gchar *str;

	socklen_t len;
} network_address;

typedef struct {
	int fd;             /**< socket-fd */
	struct event event; /**< events for this fd */

	network_address addr;

	guint32 packet_len; /**< the packet_len is a 24bit unsigned int */
	guint8  packet_id;  /**< id which increments for each packet in the stream */
	

	network_queue *recv_queue;
	network_queue *recv_raw_queue;
	network_queue *send_queue;

	unsigned char header[4]; /** raw buffer for the packet_len and packet_id */
	off_t header_read;
	off_t to_read;
	
	/**
	 * data extracted from the handshake packet 
	 *
	 * - mysqld_version and 
	 * - thread_id 
	 * are copied the client socket
	 */
	guint32 mysqld_version;  /**< numberic version of the version string */
	guint32 thread_id;       /**< connection-id, set in the handshake packet */ 
	GString *scramble_buf;   /**< the 21byte scramble-buf */

	GString *auth_handshake_packet;
	int is_authed;           /** did a client already authed this connection */
} network_socket;


network_queue *network_queue_init(void);
void network_queue_free(network_queue *queue);
int network_queue_append(network_queue *queue, const char *data, size_t len, int packet_id);
int network_queue_append_chunk(network_queue *queue, GString *chunk);

network_socket *network_socket_init(void);
void network_socket_free(network_socket *s);

#endif

