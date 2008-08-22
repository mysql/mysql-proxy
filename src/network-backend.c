/* Copyright (C) 2008 MySQL AB */ 

#include "network-backend.h"
#include "chassis-plugin.h"
#include <glib.h>

backend_t *backend_init() {
	backend_t *b;

	b = g_new0(backend_t, 1);

	b->pool = network_connection_pool_init();
	b->uuid = g_string_new(NULL);

	return b;
}

void backend_free(backend_t *b) {
	if (!b) return;

	network_connection_pool_free(b->pool);

	if (b->addr.str) g_free(b->addr.str);
	if (b->uuid)     g_string_free(b->uuid, TRUE);

	g_free(b);
}

network_backends_t *network_backends_new() {
	network_backends_t *bs;

	bs = g_new0(network_backends_t, 1);

	bs->backends = g_ptr_array_new();

	return bs;
}

void network_backends_free(network_backends_t *bs) {
	gsize i;

	if (!bs) return;

	for (i = 0; i < bs->backends->len; i++) {
		backend_t *backend = bs->backends->pdata[i];
		
		backend_free(backend);
	}

	g_ptr_array_free(bs->backends, TRUE);

	g_free(bs);
}

int network_backends_add(network_backends_t *bs, /* const */ gchar *address, backend_type_t type) {
	backend_t *new_backend;
	guint i;

	new_backend = backend_init();
	new_backend->type = type;

	if (0 != network_address_set_address(&new_backend->addr, address)) {
		return -1;
	}

	/* check if this backend is already known */
	for (i = 0; i < bs->backends->len; i++) {
		backend_t *old_backend = bs->backends->pdata[i];

		if (0 == strcmp(old_backend->addr.str, new_backend->addr.str)) {

			backend_free(new_backend);

			return -1;
		}
	}


	g_ptr_array_add(bs->backends, new_backend);

	return 0;
}

int network_backends_check(network_backends_t *backends) {
	GTimeVal now;
	guint i;

	g_get_current_time(&now);

	/* check max(once a second) */
	if (now.tv_sec - backends->backend_last_check.tv_sec < 1) return 0;

	/* check once a second if we have to wakeup a connection */
	for (i = 0; i < backends->backends->len; i++) {
		backend_t *cur = backends->backends->pdata[i];

		if (cur->state != BACKEND_STATE_DOWN) continue;

		/* check if a backend is marked as down for more than 10 sec */

		if (now.tv_sec - cur->state_since.tv_sec > 4) {
			g_debug("%s.%d: backend %s was down for more than 10 sec, waking it up", 
					__FILE__, __LINE__,
					cur->addr.str);

			cur->state = BACKEND_STATE_UNKNOWN;
			cur->state_since = now;
		}
	}

	return 0;
}

backend_t *network_backends_get(network_backends_t *backends, gint ndx) {
	if (ndx < 0) return NULL;
	if (ndx >= backends->backends->len) return NULL;
		
	return backends->backends->pdata[ndx];
}

guint network_backends_count(network_backends_t *backends) {
	return backends->backends->len;
}

