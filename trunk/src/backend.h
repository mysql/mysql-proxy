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

/**
 * Retrieve the backend pool from the proxy plugin, if it is loaded.
 *
 * @param[in] chas A pointer to the global chassis struct
 *
 * @return A pointer array to the backend_t structs
 * @retval NULL if the proxy plugin is not loaded or not uninitialized
 */
NETWORK_API GPtrArray* get_proxy_backend_pool(chassis *chas);

#endif /* _BACKEND_H_ */

