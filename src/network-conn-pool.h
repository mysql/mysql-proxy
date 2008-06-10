/* Copyright (C) 2007, 2008 MySQL AB */ 

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

NETWORK_API network_connection_pool *network_connection_pool_init(void);
NETWORK_API void network_connection_pool_free(network_connection_pool *pool);

#endif
