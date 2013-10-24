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
#ifndef __NETWORK_PACKET_H__
#define __NETWORK_PACKET_H__

#include "network-exports.h"

typedef struct {
	GString *data;

	guint offset;
} network_packet;

/**
 * create a new network packet
 */
NETWORK_API network_packet *
network_packet_new(void);

NETWORK_API void
network_packet_free(network_packet *packet);

NETWORK_API gboolean
network_packet_has_more_data(network_packet *packet, gsize len);

NETWORK_API gboolean
network_packet_skip(network_packet *packet, gsize len);

NETWORK_API gboolean
network_packet_peek_data(network_packet *packet, gpointer dst, gsize len);

NETWORK_API gboolean
network_packet_get_data(network_packet *packet, gpointer dst, gsize len);

#endif
