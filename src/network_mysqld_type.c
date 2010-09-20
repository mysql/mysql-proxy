#include <glib.h>

#include "network_mysqld_type.h"
#include "string-len.h"

/**
 * create a type that can hold a MYSQL_TYPE_LONGLONG
 */
network_mysqld_type_longlong_t *network_mysqld_type_longlong_new(void) {
	network_mysqld_type_longlong_t *ll;
	
	ll = g_slice_new0(network_mysqld_type_longlong_t);

	return ll;
}

/**
 * free a network_mysqld_type_longlong_t
 */
void network_mysqld_type_longlong_free(network_mysqld_type_longlong_t *ll) {
	if (NULL == ll) return;

	g_slice_free(network_mysqld_type_longlong_t, ll);
}

int network_mysqld_type_longlong_set(network_mysqld_type_longlong_t *ll, guint64 i) {
	ll->i = i;

	return 0;
}

/**
 * typesafe wrapper for network_mysqld_type_new()
 */
static void network_mysqld_type_data_longlong_free(network_mysqld_type_t *type) {
	if (NULL == type) return;
	if (NULL == type->data) return;

	network_mysqld_type_longlong_free(type->data);
}

/* MYSQL_TYPE_DOUBLE */

/**
 * create a type that can hold a MYSQL_TYPE_DOUBLE
 */
network_mysqld_type_double_t *network_mysqld_type_double_new(void) {
	network_mysqld_type_double_t *t;
	
	t = g_slice_new0(network_mysqld_type_double_t);

	return t;
}

/**
 * free a network_mysqld_type_double_t
 */
void network_mysqld_type_double_free(network_mysqld_type_double_t *t) {
	if (NULL == t) return;

	g_slice_free(network_mysqld_type_double_t, t);
}

static void network_mysqld_type_data_double_free(network_mysqld_type_t *type) {
	if (NULL == type) return;
	if (NULL == type->data) return;

	network_mysqld_type_double_free(type->data);
}

int network_mysqld_type_double_set(network_mysqld_type_double_t *dd, double d) {
	*dd = d;

	return 0;
}

/* MYSQL_TYPE_FLOAT */

/**
 * create a type that can hold a MYSQL_TYPE_FLOAT
 */

network_mysqld_type_float_t *network_mysqld_type_float_new(void) {
	network_mysqld_type_float_t *t;
	
	t = g_slice_new0(network_mysqld_type_float_t);

	return t;
}

/**
 * free a network_mysqld_type_float_t
 */
void network_mysqld_type_float_free(network_mysqld_type_float_t *t) {
	if (NULL == t) return;

	g_slice_free(network_mysqld_type_float_t, t);
}

static void network_mysqld_type_data_float_free(network_mysqld_type_t *type) {
	if (NULL == type) return;
	if (NULL == type->data) return;

	network_mysqld_type_float_free(type->data);
}

int network_mysqld_type_float_set(network_mysqld_type_float_t *ff, float f) {
	*ff = f;

	return 0;
}


/* MYSQL_TYPE_STRING */
network_mysqld_type_string_t *network_mysqld_type_string_new(void) {
	network_mysqld_type_string_t *str;
	
	str = g_string_new(NULL);

	return str;
}

static void network_mysqld_type_string_free(network_mysqld_type_string_t *str) {
	if (NULL == str) return;

	g_string_free(str, TRUE);
}

static void network_mysqld_type_data_string_free(network_mysqld_type_t *type) {
	if (NULL == type) return;

	network_mysqld_type_string_free(type->data);
}

/* MYSQL_TYPE_DATE */
network_mysqld_type_date_t *network_mysqld_type_date_new(void) {
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


/* MYSQL_TYPE_TIME */
network_mysqld_type_time_t *network_mysqld_type_time_new(void) {
	network_mysqld_type_time_t *t;

	t = g_slice_new0(network_mysqld_type_time_t);

	return t;
}

static void network_mysqld_type_time_free(network_mysqld_type_time_t *t) {
	if (NULL == t) return;

	g_slice_free(network_mysqld_type_time_t, t);
}

static void network_mysqld_type_data_time_free(network_mysqld_type_t *type) {
	if (NULL == type) return;

	network_mysqld_type_time_free(type->data);
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
	case MYSQL_TYPE_INT24:
		/* ->data is used as pointer */
		break;
	case MYSQL_TYPE_LONGLONG:
		type->free_data = network_mysqld_type_data_longlong_free;
		break;
	case MYSQL_TYPE_FLOAT: /* 4 bytes */
		type->free_data = network_mysqld_type_data_float_free;
		break;
	case MYSQL_TYPE_DOUBLE: /* 8 bytes */
		type->free_data = network_mysqld_type_data_double_free;
		break;
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_TIMESTAMP:
		type->free_data = network_mysqld_type_data_date_free;
		break;
	case MYSQL_TYPE_TIME:
		type->free_data = network_mysqld_type_data_time_free;
		break;
	case MYSQL_TYPE_NEWDECIMAL:
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
static int network_mysqld_type_factory_tiny_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
	guint8 i8;
	int err = 0;

	err = err || network_mysqld_proto_get_int8(packet, &i8);

	if (0 == err) {
		type->data = GINT_TO_POINTER(i8);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_tiny_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString *packet, network_mysqld_type_t *type) {
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
static int network_mysqld_type_factory_short_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
	guint16 i16;
	int err = 0;

	err = err || network_mysqld_proto_get_int16(packet, &i16);

	if (0 == err) {
		type->data = GINT_TO_POINTER(i16);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_short_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString *packet, network_mysqld_type_t *type) {
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

/* long, int24 */
static int network_mysqld_type_factory_long_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
	guint32 i32;
	int err = 0;

	err = err || network_mysqld_proto_get_int32(packet, &i32);

	if (0 == err) {
		type->data = GINT_TO_POINTER(i32);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_long_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString *packet, network_mysqld_type_t *type) {
	gint i = GPOINTER_TO_INT(type->data);

	network_mysqld_proto_append_int32(packet, i);

	return 0;
}

network_mysqld_type_factory_t *network_mysqld_type_factory_long_new(enum enum_field_types field_type) {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->to_binary = network_mysqld_type_factory_long_to_binary;
	factory->from_binary = network_mysqld_type_factory_long_from_binary;
	factory->type = field_type;

	return factory;
}

/* longlong */
static int network_mysqld_type_factory_longlong_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
	guint64 i64;
	int err = 0;

	err = err || network_mysqld_proto_get_int64(packet, &i64);

	if (0 == err) {
		network_mysqld_type_longlong_t *ll;

		ll = network_mysqld_type_longlong_new();
		network_mysqld_type_longlong_set(ll, i64);

		type->data = ll;
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_longlong_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString *packet, network_mysqld_type_t *type) {
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

/* double */
static int network_mysqld_type_factory_double_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
	int err = 0;
	union {
		double d;
		char d_char_shadow[sizeof(double) + 1];
	} double_copy;

	GString s;
	s.str = double_copy.d_char_shadow;
	s.len = 0;
	s.allocated_len = sizeof(double_copy.d_char_shadow);

	err = err || network_mysqld_proto_get_gstring_len(packet, sizeof(double), &s);

	if (0 == err) {
		network_mysqld_type_double_t *dd;

		dd = network_mysqld_type_double_new();
		network_mysqld_type_double_set(dd, double_copy.d);

		type->data = dd;
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_double_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString *packet, network_mysqld_type_t *type) {
	network_mysqld_type_double_t *dd;
	union {
		double d;
		char d_char_shadow[sizeof(double)];
	} double_copy;

	dd = type->data;

	double_copy.d = *dd;

	g_string_append_len(packet, double_copy.d_char_shadow, sizeof(double));

	return 0;
}

network_mysqld_type_factory_t *network_mysqld_type_factory_double_new() {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->type = MYSQL_TYPE_DOUBLE;

	factory->to_binary = network_mysqld_type_factory_double_to_binary;
	factory->from_binary = network_mysqld_type_factory_double_from_binary;

	return factory;
}

/* float */
static int network_mysqld_type_factory_float_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
	int err = 0;
	union {
		float d;
		char d_char_shadow[sizeof(float) + 1];
	} float_copy;

	GString s;
	s.str = float_copy.d_char_shadow;
	s.len = 0;
	s.allocated_len = sizeof(float_copy.d_char_shadow);

	err = err || network_mysqld_proto_get_gstring_len(packet, sizeof(float), &s);

	if (0 == err) {
		network_mysqld_type_float_t *dd;

		dd = network_mysqld_type_float_new();
		network_mysqld_type_float_set(dd, float_copy.d);

		type->data = dd;
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_float_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString *packet, network_mysqld_type_t *type) {
	network_mysqld_type_float_t *dd;
	union {
		float d;
		char d_char_shadow[sizeof(float)];
	} float_copy;

	dd = type->data;

	float_copy.d = *dd;

	g_string_append_len(packet, float_copy.d_char_shadow, sizeof(float));

	return 0;
}

network_mysqld_type_factory_t *network_mysqld_type_factory_float_new() {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->type = MYSQL_TYPE_DOUBLE;

	factory->to_binary = network_mysqld_type_factory_float_to_binary;
	factory->from_binary = network_mysqld_type_factory_float_from_binary;

	return factory;
}



/* all kinds of strings */
static int network_mysqld_type_factory_string_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
	int err = 0;

	type->data = network_mysqld_type_string_new();

	err = err || network_mysqld_proto_get_lenenc_gstring(packet, type->data);

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_string_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString *packet, network_mysqld_type_t *type) {
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
static int network_mysqld_type_factory_date_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
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
				err = err || network_mysqld_proto_get_int32(packet, &data->nsec);
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

static int network_mysqld_type_factory_date_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString G_GNUC_UNUSED *packet, network_mysqld_type_t G_GNUC_UNUSED *type) {
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
 * extract the time from a binary resultset row
 */
static int network_mysqld_type_factory_time_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
	int err = 0;
	guint8 len;
	network_mysqld_type_time_t *data;

	err = err || network_mysqld_proto_get_int8(packet, &len);

	/* check the valid len's
	 *
	 * sadly we can't use fallthrough here as we can only process the packets left-to-right
	 */
	switch (len) {
	case 12: /* day + time + ms */
	case 8:  /* day + time ( ms is .0000 ) */
	case 0:  /* time == 00:00:00 */
		break;
	default:
		return -1;
	}

	data = network_mysqld_type_time_new();

	if (len > 0) {
		err = err || network_mysqld_proto_get_int8(packet, &data->sign);
		err = err || network_mysqld_proto_get_int32(packet, &data->days);
		
		err = err || network_mysqld_proto_get_int8(packet, &data->hour);
		err = err || network_mysqld_proto_get_int8(packet, &data->min);
		err = err || network_mysqld_proto_get_int8(packet, &data->sec);

		if (len > 8) {
			err = err || network_mysqld_proto_get_int32(packet, &data->nsec);
		}
	}

	if (0 == err) {
		type->data = data;
	} else {
		network_mysqld_type_time_free(data);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_time_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString G_GNUC_UNUSED *packet, network_mysqld_type_t G_GNUC_UNUSED *type) {
	return -1;
}

network_mysqld_type_factory_t *network_mysqld_type_factory_time_new(void) {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->type = MYSQL_TYPE_TIME;

	factory->to_binary = network_mysqld_type_factory_time_to_binary;
	factory->from_binary = network_mysqld_type_factory_time_from_binary;

	return factory;
}


/**
 * valid types for prepared statements parameters we receive from the client
 */
gboolean network_mysql_type_is_valid_binary_input(enum enum_field_types field_type) {
	switch (field_type) {
	case MYSQL_TYPE_TINY:
	case MYSQL_TYPE_SHORT:
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_LONGLONG:

	case MYSQL_TYPE_FLOAT:
	case MYSQL_TYPE_DOUBLE:

	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_STRING:

	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_TIME:
	case MYSQL_TYPE_TIMESTAMP:

	case MYSQL_TYPE_NULL:
		return TRUE;
	default:
		return FALSE;
	}
}

/**
 * types we allow the send back to the client
 */
gboolean network_mysql_type_is_valid_binary_output(enum enum_field_types field_type) {
	switch (field_type) {
	case MYSQL_TYPE_TINY:
	case MYSQL_TYPE_SHORT:
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_LONGLONG:

	case MYSQL_TYPE_FLOAT:
	case MYSQL_TYPE_DOUBLE:
	case MYSQL_TYPE_NEWDECIMAL:

	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_LONG_BLOB:
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:

	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_TIME:
	case MYSQL_TYPE_TIMESTAMP:

	case MYSQL_TYPE_BIT:
		return TRUE;
	default:
		return FALSE;
	}
}

/**
 * create a factory for types 
 */
network_mysqld_type_factory_t *network_mysqld_type_factory_new(enum enum_field_types field_type) {
	switch (field_type) {
	case MYSQL_TYPE_TINY:
		return network_mysqld_type_factory_tiny_new();
	case MYSQL_TYPE_SHORT:
		return network_mysqld_type_factory_short_new();
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_INT24:
		return network_mysqld_type_factory_long_new(field_type);
	case MYSQL_TYPE_LONGLONG:
		return network_mysqld_type_factory_longlong_new();
	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_TIMESTAMP:
		return network_mysqld_type_factory_date_new(field_type);
	case MYSQL_TYPE_TIME:
		return network_mysqld_type_factory_time_new();
	case MYSQL_TYPE_FLOAT:
		return network_mysqld_type_factory_float_new();
	case MYSQL_TYPE_DOUBLE:
		return network_mysqld_type_factory_double_new();
	case MYSQL_TYPE_BIT:
	case MYSQL_TYPE_NEWDECIMAL:
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_LONG_BLOB:
	case MYSQL_TYPE_STRING:
	case MYSQL_TYPE_VAR_STRING:
		/* they are all length-encoded strings */
		return network_mysqld_type_factory_string_new(field_type);
	}

	/* our default */
	g_debug("%s: we don't have a factory for type = %d, yet", G_STRLOC, field_type);
	return NULL;
}

/**
 * shut down the factory
 */
void network_mysqld_type_factory_free(network_mysqld_type_factory_t *factory) {
	if (NULL == factory) return;

	g_slice_free(network_mysqld_type_factory_t, factory);
}

