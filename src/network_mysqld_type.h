#ifndef __NETWORK_MYSQLD_TYPE_H__
#define __NETWORK_MYSQLD_TYPE_H__

#include <mysql.h>
#include <glib.h>

#include "network-mysqld-proto.h"

typedef struct _network_mysqld_type_t network_mysqld_type_t;

struct _network_mysqld_type_t{
	enum enum_field_types type;

	gpointer data;
	void (*free_data)(network_mysqld_type_t *type);

	gboolean is_null;
	gboolean is_unsigned;
}; 

NETWORK_API network_mysqld_type_t *network_mysqld_type_new(enum enum_field_types _type);
NETWORK_API void network_mysqld_type_free(network_mysqld_type_t *type);

/* expose the types itself and their internal representation */

typedef double network_mysqld_type_double_t;
network_mysqld_type_double_t *network_mysqld_type_double_new(void);
void network_mysqld_type_double_free(network_mysqld_type_double_t *t);

typedef float network_mysqld_type_float_t;

typedef GString network_mysqld_type_string_t;

typedef struct {
	guint16 year;
	guint8  month;
	guint8  day;
	
	guint8  hour;
	guint8  min;
	guint8  sec;

	guint32 nsec; /* the nano-second part */
} network_mysqld_type_date_t;

typedef struct {
	guint8  sign;
	guint32 days;
	
	guint8  hour;
	guint8  min;
	guint8  sec;

	guint32 nsec; /* the nano-second part */
} network_mysqld_type_time_t;

typedef struct {
	guint64 i;
	gboolean is_unsigned;
} network_mysqld_type_longlong_t;

/**
 * factory for types
 *
 * generate types from the binary or text protocol
 */
typedef struct _network_mysqld_type_factory_t network_mysqld_type_factory_t;

struct _network_mysqld_type_factory_t {
	enum enum_field_types type;

	/* to binary protocol */
	int (*to_binary)(network_mysqld_type_factory_t *factory, GString *packet, network_mysqld_type_t *type);
	int (*from_binary)(network_mysqld_type_factory_t *factory, network_packet *packet, network_mysqld_type_t *type);
#if 0
	/* will be added later */
	int (*to_text)(network_mysqld_type_factory_t *factory, GString *packet, network_mysqld_type_t *type);
	int (*from_text)(network_mysqld_type_factory_t *factory, network_packet *packet, network_mysqld_type_t *type);
#endif
};

NETWORK_API network_mysqld_type_factory_t *network_mysqld_type_factory_new(enum enum_field_types _type);
NETWORK_API void network_mysqld_type_factory_free(network_mysqld_type_factory_t *factory);

#endif
