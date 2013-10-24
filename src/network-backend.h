/* $%BEGINLICENSE%$
 Copyright (c) 2008, 2012, Oracle and/or its affiliates. All rights reserved.

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
	network_address *addr;
   
	backend_state_t state;   /**< UP or DOWN */
	backend_type_t type;     /**< ReadWrite or ReadOnly */

	GTimeVal state_since;    /**< timestamp of the last state-change */

	network_connection_pool *pool; /**< the pool of open connections */

	guint connected_clients; /**< number of open connections to this backend for SQF */

	GString *uuid;           /**< the UUID of the backend */
} network_backend_t;

typedef network_backend_t backend_t G_GNUC_DEPRECATED;

NETWORK_API network_backend_t *backend_init() G_GNUC_DEPRECATED;
NETWORK_API void backend_free(network_backend_t *b) G_GNUC_DEPRECATED;

NETWORK_API network_backend_t *network_backend_new();
NETWORK_API void network_backend_free(network_backend_t *b);

typedef struct {
	GPtrArray *backends;
	GMutex    *backends_mutex;
	
	GTimeVal backend_last_check;
} network_backends_t;

NETWORK_API network_backends_t *network_backends_new();
NETWORK_API void network_backends_free(network_backends_t *);
NETWORK_API int network_backends_add(network_backends_t *backends, /* const */ gchar *address, backend_type_t type);
NETWORK_API int network_backends_check(network_backends_t *backends);
NETWORK_API network_backend_t * network_backends_get(network_backends_t *backends, guint ndx);
NETWORK_API guint network_backends_count(network_backends_t *backends);

#endif /* _BACKEND_H_ */

