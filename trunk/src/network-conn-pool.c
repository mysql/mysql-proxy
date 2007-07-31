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

#include <glib.h>

#include "network-conn-pool.h"

network_connection_pool *network_connection_pool_init(void) {
	network_connection_pool *pool;

	pool = g_new0(network_connection_pool, 1);

	pool->entries = g_ptr_array_new();

	return pool;
}

void network_connection_pool_free(network_connection_pool *pool) {
	if (!pool) return;

	g_ptr_array_free(pool->entries, TRUE);

	g_free(pool);
}

network_socket *network_connection_pool_get(network_connection_pool *pool) {
	GPtrArray *arr = pool->entries;

	network_connection_pool_entry *entry;
	network_socket *sock;

	if (arr->len == 0) return NULL;

	entry = g_ptr_array_remove_index_fast(pool->entries, 0);
	sock = entry->srv_sock;
	g_free(entry);

	/* remove the idle handler from the socket */	
	event_del(&(sock->event));

	return sock;
}

network_connection_pool_entry *network_connection_pool_add(network_connection_pool *pool, network_socket *sock) {
	network_connection_pool_entry *entry;

	entry = g_new0(network_connection_pool_entry, 1);

	entry->srv_sock = sock;
	entry->pool = pool;

	g_ptr_array_add(pool->entries, entry);

	return entry;
}

void network_connection_pool_remove(network_connection_pool *pool, network_connection_pool_entry *entry) {
	GPtrArray *arr = pool->entries;
	gsize i;

	for (i = 0; i < arr->len; i++) {
		network_connection_pool_entry *aentry = arr->pdata[i];

		if (!aentry) continue;

		if (aentry == entry) {
			g_free(entry);

			g_ptr_array_remove_index_fast(pool->entries, i);

			return;
		}
	}
}

