#ifndef __NETWORK_MYSQLD_TYPE_H__
#define __NETWORK_MYSQLD_TYPE_H__

#include <mysql.h>
#include <glib.h>

#include "network-mysqld-proto.h"

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

typedef struct _network_mysqld_type_t network_mysqld_type_t;

struct _network_mysqld_type_t {
	enum enum_field_types type;

	gpointer data;
	void (*free_data)(network_mysqld_type_t *type);

	int (*get_gstring)(network_mysqld_type_t *type, GString *s);
	int (*get_string_const)(network_mysqld_type_t *type, const char **s, gsize *s_len);
	int (*get_string)(network_mysqld_type_t *type, char **s, gsize *len);
	int (*set_string)(network_mysqld_type_t *type, const char *s, gsize s_len);
	int (*get_int)(network_mysqld_type_t *type, guint64 *i, gboolean *is_unsigned);
	int (*set_int)(network_mysqld_type_t *type, guint64 i, gboolean is_unsigned);
	int (*get_double)(network_mysqld_type_t *type, double *d);
	int (*set_double)(network_mysqld_type_t *type, double d);
	int (*get_date)(network_mysqld_type_t *type, network_mysqld_type_date_t *date);
	int (*set_date)(network_mysqld_type_t *type, network_mysqld_type_date_t *date);
	int (*get_time)(network_mysqld_type_t *type, network_mysqld_type_time_t *t);
	int (*set_time)(network_mysqld_type_t *type, network_mysqld_type_time_t *t);


	gboolean is_null;
	gboolean is_unsigned;
}; 

NETWORK_API network_mysqld_type_t *network_mysqld_type_new(enum enum_field_types _type);
NETWORK_API void network_mysqld_type_free(network_mysqld_type_t *type);

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
