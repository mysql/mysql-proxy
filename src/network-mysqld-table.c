/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <glib.h>

#include "network-mysqld.h"

network_mysqld_table *network_mysqld_table_init(void) {
	network_mysqld_table *t;

	t = g_new0(network_mysqld_table, 1);

	return t;
}

void network_mysqld_table_free(network_mysqld_table *t) {
	if (!t) return;

	g_free(t);
}
