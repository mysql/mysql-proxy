#ifndef __NETWORK_MYSQLD_TYPE_H__
#define __NETWORK_MYSQLD_TYPE_H__

#include <mysql.h>
#include <glib.h>

#include "network-mysqld-proto.h"

typedef struct _network_mysqld_type_t network_mysqld_type_t;

struct _network_mysqld_type_t {
	enum enum_field_types type;

	/* to binary protocol */
	int (*to_binary)(network_mysqld_type_t *type, GString *packet);
	int (*from_binary)(network_mysqld_type_t *type, network_packet *packet);

	int (*to_text)(network_mysqld_type_t *type, GString *packet);
	int (*from_text)(network_mysqld_type_t *type, network_packet *packet);

	gpointer data;
	void (*free_data)(network_mysqld_type_t *type);

	gboolean is_null;
};

network_mysqld_type_t *network_mysqld_type_new(enum enum_field_types _type);
void network_mysqld_type_free(network_mysqld_type_t *type);

#endif
