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

#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>
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

int network_queue_append(network_queue *queue, const char *data, size_t len, int packet_id) {
	unsigned char header[4];
	GString *s;

	network_mysqld_proto_set_header(header, len, packet_id);

	s = g_string_sized_new(len + 4);

	g_string_append_len(s, (gchar *)header, 4);
	g_string_append_len(s, data, len);

	g_queue_push_tail(queue->chunks, s);

	return 0;
}

int network_queue_append_chunk(network_queue *queue, GString *chunk) {
	g_queue_push_tail(queue->chunks, chunk);

	return 0;
}

network_socket *network_socket_init() {
	network_socket *s;
	
	s = g_new0(network_socket, 1);

	s->send_queue = network_queue_init();
	s->recv_queue = network_queue_init();
	s->recv_raw_queue = network_queue_init();

	s->packet_len = PACKET_LEN_UNSET;

	s->scramble_buf = g_string_new(NULL);
	s->auth_handshake_packet = g_string_new(NULL);
	
	return s;
}

void network_socket_free(network_socket *s) {
	if (!s) return;

	network_queue_free(s->send_queue);
	network_queue_free(s->recv_queue);
	network_queue_free(s->recv_raw_queue);
#if 0
	/* */
	if (s->addr.str) {
		g_free(s->addr.str);
	}
#endif

	if (s->fd != -1) {
		closesocket(s->fd);
	}

	g_string_free(s->scramble_buf, 1);
	g_string_free(s->auth_handshake_packet, 1);

	g_free(s);
}


