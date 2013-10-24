/* $%BEGINLICENSE%$
 Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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
#include <glib.h>
#include <stdlib.h>
#include <string.h>

#include "network-packet.h"

network_packet *
network_packet_new(void) {
	network_packet *packet;

	packet = g_new0(network_packet, 1);

	return packet;
}

void
network_packet_free(network_packet *packet) {
	if (!packet) return;

	g_free(packet);
}

gboolean
network_packet_has_more_data(network_packet *packet, gsize len) {
	if (packet->offset > packet->data->len) return FALSE; /* we are already out of bounds, shouldn't happen */
	if (len > packet->data->len - packet->offset) return FALSE;

	return TRUE;
}

gboolean
network_packet_skip(network_packet *packet, gsize len) {
	if (!network_packet_has_more_data(packet, len)) {
		return FALSE;
	}

	packet->offset += len;
	return TRUE;
}

gboolean
network_packet_peek_data(network_packet *packet, gpointer dst, gsize len) {
	if (!network_packet_has_more_data(packet, len)) return FALSE;

	memcpy(dst, packet->data->str + packet->offset, len);

	return TRUE;
}


gboolean
network_packet_get_data(network_packet *packet, gpointer dst, gsize len) {
	if (!network_packet_peek_data(packet, dst, len)) {
		return FALSE;
	}

	packet->offset += len;

	return TRUE;
}

