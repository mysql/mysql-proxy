/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifndef _PROXY_PLUGIN_H
#define	_PROXY_PLUGIN_H

/**
 * the shared information across all connections 
 *
 */
typedef struct {
	/**
	 * our pool of backends
	 *
	 * GPtrArray<backend_t>
	 */
	GPtrArray *backend_pool; 

	GTimeVal backend_last_check;
} proxy_global_state_t;


#endif	/* _PROXY_PLUGIN_H */

