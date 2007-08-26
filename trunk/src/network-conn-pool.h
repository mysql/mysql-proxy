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

#ifndef _NETWORK_CONN_POOL_H_
#define _NETWORK_CONN_POOL_H_

#include <glib.h>

#include "network-socket.h"

typedef struct {
	GPtrArray *entries;
} network_connection_pool;

typedef struct {
	network_socket *srv_sock;
	
	network_connection_pool *pool;
} network_connection_pool_entry;

network_socket *network_connection_pool_get(network_connection_pool *pool,
		GString *username,
		GString *default_db);
network_connection_pool_entry *network_connection_pool_add(network_connection_pool *pool, network_socket *sock);
void network_connection_pool_remove(network_connection_pool *pool, network_connection_pool_entry *entry);

network_connection_pool *network_connection_pool_init(void);
void network_connection_pool_free(network_connection_pool *pool);

#endif
