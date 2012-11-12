/* $%BEGINLICENSE%$
 Copyright (c) 2007, 2012, Oracle and/or its affiliates. All rights reserved.

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
 

#ifndef _NETWORK_CONN_POOL_H_
#define _NETWORK_CONN_POOL_H_

#include <glib.h>

#include "network-socket.h"
#include "network-exports.h"

typedef struct {
	GHashTable *users; /** GHashTable<GString, GQueue<network_connection_pool_entry>> */
	
	guint max_idle_connections;
	guint min_idle_connections;
} network_connection_pool;

typedef struct {
	network_socket *sock;          /** the idling socket */
	
	network_connection_pool *pool; /** a pointer back to the pool */

	GTimeVal added_ts;             /** added at ... we want to make sure we don't hit wait_timeout */
} network_connection_pool_entry;

NETWORK_API network_socket *network_connection_pool_get(network_connection_pool *pool,
		GString *username,
		GString *default_db);
NETWORK_API network_connection_pool_entry *network_connection_pool_add(network_connection_pool *pool, network_socket *sock);
NETWORK_API void network_connection_pool_remove(network_connection_pool *pool, network_connection_pool_entry *entry);
NETWORK_API GQueue *network_connection_pool_get_conns(network_connection_pool *pool, GString *username, GString *);

NETWORK_API network_connection_pool *network_connection_pool_init(void) G_GNUC_DEPRECATED;
NETWORK_API network_connection_pool *network_connection_pool_new(void);
NETWORK_API void network_connection_pool_free(network_connection_pool *pool);

#endif
