/* Copyright (C) 2008 MySQL AB
 
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

#ifndef _BACKEND_H_
#define _BACKEND_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "network-conn-pool.h"
#include "chassis-mainloop.h"

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


backend_t *backend_init();
void backend_free(backend_t *b);

/**
 * Retrieve the backend pool from the proxy plugin, if it is loaded.
 *
 * @param[in] chas A pointer to the global chassis struct
 *
 * @return A pointer array to the backend_t structs
 * @retval NULL if the proxy plugin is not loaded or not uninitialized
 */
GPtrArray* get_proxy_backend_pool(chassis *chas);

#endif /* _BACKEND_H_ */

