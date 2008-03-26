/* Copyright (C) 2008 MySQL AB */ 

#include "backend.h"
#include "chassis-plugin.h"
#include <glib.h>

backend_t *backend_init() {
	backend_t *b;
    
	b = g_new0(backend_t, 1);
    
	b->pool = network_connection_pool_init();
    
	return b;
}

void backend_free(backend_t *b) {
	if (!b) return;
    
	network_connection_pool_free(b->pool);
    
	if (b->addr.str) g_free(b->addr.str);
    
	g_free(b);
}

GPtrArray* get_proxy_backend_pool(chassis *chas) {
    chassis_plugin *proxy_plugin = chassis_plugin_for_name(chas, "proxy");

    /* if there's no proxy, there's no backend_pool */
    if (!proxy_plugin) {
		g_critical("the proxy plugin has not been loaded, cannot retrieve backend pool");
        return NULL;
    }
    return (GPtrArray*)proxy_plugin->get_global_state(proxy_plugin->config, "backend_pool");
}
