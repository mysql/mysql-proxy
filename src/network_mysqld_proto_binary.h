/* $%BEGINLICENSE%$
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
#ifndef __NETWORK_MYSQLD_PROTO_BINARY_H__
#define __NETWORK_MYSQLD_PROTO_BINARY_H__

#include <glib.h>

#include "network-socket.h"
#include "network_mysqld_type.h"

#include "network-exports.h"

NETWORK_API int network_mysqld_proto_binary_get_type(network_packet *packet, network_mysqld_type_t *type);
NETWORK_API int network_mysqld_proto_binary_append_type(GString *packet, network_mysqld_type_t *type);

#endif
