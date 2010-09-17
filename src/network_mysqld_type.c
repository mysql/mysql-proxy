#include <glib.h>

#include "network_mysqld_type.h"
#include "string-len.h"

static void network_mysqld_type_data_longlong_free(network_mysqld_type_t *type) {
	if (NULL == type) return;
	if (NULL == type->data) return;

	g_slice_free(guint64, type->data);
}

typedef GString network_mysqld_type_string_t;

static void network_mysqld_type_string_free(network_mysqld_type_string_t *str) {
	if (NULL == str) return;

	g_string_free(str, TRUE);
}

static void network_mysqld_type_data_string_free(network_mysqld_type_t *type) {
	if (NULL == type) return;

	network_mysqld_type_string_free(type->data);
}

typedef struct {
	guint16 year;
	guint8  month;
	guint8  day;
	
	guint8  hour;
	guint8  min;
	guint8  sec;

	guint32 sub_sec; /* is that ms ? us ? */
} network_mysqld_type_date_t;

network_mysqld_type_date_t *network_mysqld_type_date_new() {
	network_mysqld_type_date_t *date;

	date = g_slice_new0(network_mysqld_type_date_t);

	return date;
}

static void network_mysqld_type_date_free(network_mysqld_type_date_t *date) {
	if (NULL == date) return;

	g_slice_free(network_mysqld_type_date_t, date);
}

static void network_mysqld_type_data_date_free(network_mysqld_type_t *type) {
	if (NULL == type) return;

	network_mysqld_type_date_free(type->data);
}

/**
 * create a type 
 */
network_mysqld_type_t *network_mysqld_type_new(enum enum_field_types field_type) {
	network_mysqld_type_t *type;

	type = g_slice_new0(network_mysqld_type_t);
	type->type = field_type;

	switch (field_type) {
	case MYSQL_TYPE_TINY:
	case MYSQL_TYPE_SHORT:
	case MYSQL_TYPE_LONG:
		/* ->data is used as pointer */
		break;
	case MYSQL_TYPE_LONGLONG:
		type->free_data = network_mysqld_type_data_longlong_free;
		break;
	case MYSQL_TYPE_FLOAT: /* 4 bytes */
	case MYSQL_TYPE_DOUBLE: /* 8 bytes */
	case MYSQL_TYPE_DECIMAL:
	case MYSQL_TYPE_NEWDECIMAL:
	case MYSQL_TYPE_TIME:
	case MYSQL_TYPE_DATETIME:
		g_assert_not_reached();
		break;
	case MYSQL_TYPE_DATE:
		type->free_data = network_mysqld_type_data_date_free;
		break;
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_LONG_BLOB:
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
	case MYSQL_TYPE_VARCHAR:
		/* they are all length-encoded strings */
		type->free_data = network_mysqld_type_data_string_free;
		break;
	}

	return type;
}
/**
 * free a type
 */
void network_mysqld_type_free(network_mysqld_type_t *type) {
	if (NULL == type) return;

	if (NULL != type->free_data) {
		type->free_data(type);
	}
	g_slice_free(network_mysqld_type_t, type);
}

/* tiny */
static int network_mysqld_type_factory_tiny_from_binary(network_mysqld_type_factory_t *factory, network_packet *packet, network_mysqld_type_t *type) {
	guint8 i8;
	int err;

	err = err || network_mysqld_proto_get_int8(packet, &i8);

	if (0 == err) {
		type->data = GINT_TO_POINTER(i8);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_tiny_to_binary(network_mysqld_type_factory_t *factory, GString *packet, network_mysqld_type_t *type) {
	gint i = GPOINTER_TO_INT(type->data);

	network_mysqld_proto_append_int8(packet, i);

	return 0;
}

/** 
 * create a factory for TINYs 
 */
network_mysqld_type_factory_t *network_mysqld_type_factory_tiny_new(void) {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->to_binary = network_mysqld_type_factory_tiny_to_binary;
	factory->from_binary = network_mysqld_type_factory_tiny_from_binary;
	factory->type = MYSQL_TYPE_TINY;

	return factory;
}

/* short */
static int network_mysqld_type_factory_short_from_binary(network_mysqld_type_factory_t *factory, network_packet *packet, network_mysqld_type_t *type) {
	guint16 i16;
	int err;

	err = err || network_mysqld_proto_get_int16(packet, &i16);

	if (0 == err) {
		type->data = GINT_TO_POINTER(i16);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_short_to_binary(network_mysqld_type_factory_t *factory, GString *packet, network_mysqld_type_t *type) {
	gint i = GPOINTER_TO_INT(type->data);

	network_mysqld_proto_append_int16(packet, i);

	return 0;
}

network_mysqld_type_factory_t *network_mysqld_type_factory_short_new() {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->to_binary = network_mysqld_type_factory_short_to_binary;
	factory->from_binary = network_mysqld_type_factory_short_from_binary;
	factory->type = MYSQL_TYPE_SHORT;

	return factory;
}

/* long */
static int network_mysqld_type_factory_long_from_binary(network_mysqld_type_factory_t *factory, network_packet *packet, network_mysqld_type_t *type) {
	guint32 i32;
	int err;

	err = err || network_mysqld_proto_get_int32(packet, &i32);

	if (0 == err) {
		type->data = GINT_TO_POINTER(i32);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_long_to_binary(network_mysqld_type_factory_t *factory, GString *packet, network_mysqld_type_t *type) {
	gint i = GPOINTER_TO_INT(type->data);

	network_mysqld_proto_append_int32(packet, i);

	return 0;
}

network_mysqld_type_factory_t *network_mysqld_type_factory_long_new() {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->to_binary = network_mysqld_type_factory_long_to_binary;
	factory->from_binary = network_mysqld_type_factory_long_from_binary;
	factory->type = MYSQL_TYPE_SHORT;

	return factory;
}

/* longlong */
static int network_mysqld_type_factory_longlong_from_binary(network_mysqld_type_factory_t *factory, network_packet *packet, network_mysqld_type_t *type) {
	guint64 i64;
	int err;

	err = err || network_mysqld_proto_get_int64(packet, &i64);

	if (0 == err) {
		type->data = g_slice_new(guint64);
		*(guint64 *)(type->data) = i64;
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_longlong_to_binary(network_mysqld_type_factory_t *factory, GString *packet, network_mysqld_type_t *type) {
	guint64 i64;

	i64 = *(guint64 *)(type->data); 

	network_mysqld_proto_append_int64(packet, i64);

	return 0;
}

network_mysqld_type_factory_t *network_mysqld_type_factory_longlong_new() {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->type = MYSQL_TYPE_LONGLONG;

	factory->to_binary = network_mysqld_type_factory_longlong_to_binary;
	factory->from_binary = network_mysqld_type_factory_longlong_from_binary;

	return factory;
}

/* all kinds of strings */
static int network_mysqld_type_factory_string_from_binary(network_mysqld_type_factory_t *factory, network_packet *packet, network_mysqld_type_t *type) {
	int err = 0;

	type->data = g_string_new(NULL);

	err = err || network_mysqld_proto_get_lenenc_gstring(packet, type->data);

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_string_to_binary(network_mysqld_type_factory_t *factory, GString *packet, network_mysqld_type_t *type) {
	GString *data = type->data;

	network_mysqld_proto_append_lenenc_string_len(packet, S(data));

	return 0;
}

network_mysqld_type_factory_t *network_mysqld_type_factory_string_new(enum enum_field_types field_type) {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->type = field_type;

	factory->to_binary = network_mysqld_type_factory_string_to_binary;
	factory->from_binary = network_mysqld_type_factory_string_from_binary;

	return factory;
}

/* all kinds of time */

/**
 * extract the date from a binary resultset row
 */
static int network_mysqld_type_factory_date_from_binary(network_mysqld_type_factory_t *factory, network_packet *packet, network_mysqld_type_t *type) {
	int err = 0;
	guint8 len;
	network_mysqld_type_date_t *data;

	err = err || network_mysqld_proto_get_int8(packet, &len);

	/* check the valid len's
	 *
	 * sadly we can't use fallthrough here as we can only process the packets left-to-right
	 */
	switch (len) {
	case 11: /* date + time + ms */
	case 7:  /* date + time ( ms is .0000 ) */
	case 4:  /* date ( time is 00:00:00 )*/
	case 0:  /* date == 0000-00-00 */
		break;
	default:
		return -1;
	}

	data = network_mysqld_type_date_new();

	if (len > 0) {
		err = err || network_mysqld_proto_get_int16(packet, &data->year);
		err = err || network_mysqld_proto_get_int8(packet, &data->month);
		err = err || network_mysqld_proto_get_int8(packet, &data->day);
		
		if (len > 4) {
			err = err || network_mysqld_proto_get_int8(packet, &data->hour);
			err = err || network_mysqld_proto_get_int8(packet, &data->min);
			err = err || network_mysqld_proto_get_int8(packet, &data->sec);

			if (len > 7) {
				err = err || network_mysqld_proto_get_int32(packet, &data->sub_sec);
			}
		}
	}

	if (!err) {
		type->data = data;
	} else {
		network_mysqld_type_date_free(data);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_date_to_binary(network_mysqld_type_factory_t *factory, GString *packet, network_mysqld_type_t *type) {
	return -1;
}

network_mysqld_type_factory_t *network_mysqld_type_factory_date_new(enum enum_field_types field_type) {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->type = field_type;

	factory->to_binary = network_mysqld_type_factory_date_to_binary;
	factory->from_binary = network_mysqld_type_factory_date_from_binary;

	return factory;
}

/**
 * create a factory for types 
 */
network_mysqld_type_factory_t *network_mysqld_type_factory_new(enum enum_field_types type) {
	switch (type) {
	case MYSQL_TYPE_TINY:
		return network_mysqld_type_factory_tiny_new();
	case MYSQL_TYPE_SHORT:
		return network_mysqld_type_factory_short_new();
	case MYSQL_TYPE_LONG:
		return network_mysqld_type_factory_long_new();
	case MYSQL_TYPE_LONGLONG:
		return network_mysqld_type_factory_longlong_new();
	case MYSQL_TYPE_DATE:
		return network_mysqld_type_factory_date_new(type);
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_LONG_BLOB:
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
	case MYSQL_TYPE_VARCHAR:
		/* they are all length-encoded strings */
		return network_mysqld_type_factory_string_new(type);
	}

	/* our default */
	g_error("%s: we don't have a factory for type = %d, yet", G_STRLOC, type);
	return NULL;
}

/**
 * shut down the factory
 */
void network_mysqld_type_factory_free(network_mysqld_type_factory_t *factory) {
	if (NULL == factory) return;

	g_slice_free(network_mysqld_type_factory_t, factory);
}

