/* Copyright (C) 2008 MySQL AB */ 

#ifndef _BACKEND_H_
#define _BACKEND_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "network-conn-pool.h"
#include "chassis-mainloop.h"

#include "network-exports.h"

typedef enum { 
	BACKEND_STATE_UNKNOWN, 
	BACKEND_STATE_UP, 
	BACKEND_STATE_DOWN
} backend_state_t;

typedef enum { 
	BACKEND_TYPE_UNKNOWN, 
	BACKEND_TYPE_RW, 
	BACKEND_TYPE_RO
} backend_type_t;

typedef struct {
	network_address addr;
    
	backend_state_t state;   /**< UP or DOWN */
	backend_type_t type;     /**< ReadWrite or ReadOnly */
    
	GTimeVal state_since;    /**< timestamp of the last state-change */
    
	network_connection_pool *pool; /**< the pool of open connections */
    
	guint connected_clients; /**< number of open connections to this backend for SQF */
} backend_t;


NETWORK_API backend_t *backend_init();
NETWORK_API void backend_free(backend_t *b);

typedef struct {
	GPtrArray *backends;
	
	GTimeVal backend_last_check;
} network_backends_t;

NETWORK_API network_backends_t *network_backends_new();
NETWORK_API void network_backends_free(network_backends_t *);
NETWORK_API int network_backends_add(network_backends_t *backends, /* const */ gchar *address, backend_type_t type);
NETWORK_API int network_backends_check(network_backends_t *backends);
NETWORK_API backend_t * network_backends_get(network_backends_t *backends, gint ndx);
NETWORK_API guint network_backends_count(network_backends_t *backends);

#endif /* _BACKEND_H_ */

