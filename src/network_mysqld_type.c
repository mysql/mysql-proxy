#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include "network_mysqld_type.h"
#include "string-len.h"

#include "glib-ext.h"

/* expose the types itself and their internal representation */

typedef double network_mysqld_type_double_t;

typedef float network_mysqld_type_float_t;

typedef GString network_mysqld_type_string_t;

typedef struct {
	guint64 i;
	gboolean is_unsigned;
} network_mysqld_type_int_t;

/**
 * create a type that can hold a MYSQL_TYPE_LONGLONG
 */
static network_mysqld_type_int_t *network_mysqld_type_int_new(void) {
	network_mysqld_type_int_t *ll;
	
	ll = g_slice_new0(network_mysqld_type_int_t);

	return ll;
}

/**
 * free a network_mysqld_type_int_t
 */
static void network_mysqld_type_int_free(network_mysqld_type_int_t *ll) {
	if (NULL == ll) return;

	g_slice_free(network_mysqld_type_int_t, ll);
}

static int network_mysqld_type_data_int_get_int(network_mysqld_type_t *type, guint64 *i, gboolean *is_unsigned) {
	network_mysqld_type_int_t *value;

	if (NULL == type->data) return -1;

	value = type->data;

	*i = value->i;
	*is_unsigned = value->is_unsigned;

	return 0;
}

static int network_mysqld_type_data_int_set_int(network_mysqld_type_t *type, guint64 i, gboolean is_unsigned) {
	network_mysqld_type_int_t *value;

	if (NULL == type->data) {
		type->data = network_mysqld_type_int_new();
	}	
	value = type->data;

	value->i = i;
	value->is_unsigned = is_unsigned;

	return 0;
}


/**
 * typesafe wrapper for network_mysqld_type_new()
 */
static void network_mysqld_type_data_int_free(network_mysqld_type_t *type) {
	if (NULL == type) return;
	if (NULL == type->data) return;

	network_mysqld_type_int_free(type->data);
}

static void network_mysqld_type_data_int_init(network_mysqld_type_t *type, enum enum_field_types field_type) {
	type->type	= field_type;
	type->free_data = network_mysqld_type_data_int_free;
	type->get_int    = network_mysqld_type_data_int_get_int;
	type->set_int    = network_mysqld_type_data_int_set_int;
}

/* MYSQL_TYPE_DOUBLE */

/**
 * create a type that can hold a MYSQL_TYPE_DOUBLE
 */
static network_mysqld_type_double_t *network_mysqld_type_double_new(void) {
	network_mysqld_type_double_t *t;
	
	t = g_slice_new0(network_mysqld_type_double_t);

	return t;
}

/**
 * free a network_mysqld_type_double_t
 */
static void network_mysqld_type_double_free(network_mysqld_type_double_t *t) {
	if (NULL == t) return;

	g_slice_free(network_mysqld_type_double_t, t);
}

static void network_mysqld_type_data_double_free(network_mysqld_type_t *type) {
	if (NULL == type) return;
	if (NULL == type->data) return;

	network_mysqld_type_double_free(type->data);
}

static int network_mysqld_type_data_double_get_double(network_mysqld_type_t *type, double *d) {
	network_mysqld_type_double_t *value = type->data;

	if (NULL == value) return -1;

	*d = *value;

	return 0;
}

static int network_mysqld_type_data_double_set_double(network_mysqld_type_t *type, double d) {
	network_mysqld_type_double_t *value;

	if (NULL == type->data) {
		type->data = network_mysqld_type_double_new();
	}

	value = type->data;
	*value = d;

	return 0;
}

static void network_mysqld_type_data_double_init(network_mysqld_type_t *type, enum enum_field_types field_type) {
	type->type	= field_type;
	type->free_data = network_mysqld_type_data_double_free;
	type->get_double = network_mysqld_type_data_double_get_double;
	type->set_double = network_mysqld_type_data_double_set_double;
}

/* MYSQL_TYPE_FLOAT */

/**
 * create a type that can hold a MYSQL_TYPE_FLOAT
 */

static network_mysqld_type_float_t *network_mysqld_type_float_new(void) {
	network_mysqld_type_float_t *t;
	
	t = g_slice_new0(network_mysqld_type_float_t);

	return t;
}

/**
 * free a network_mysqld_type_float_t
 */
static void network_mysqld_type_float_free(network_mysqld_type_float_t *t) {
	if (NULL == t) return;

	g_slice_free(network_mysqld_type_float_t, t);
}

static void network_mysqld_type_data_float_free(network_mysqld_type_t *type) {
	if (NULL == type) return;
	if (NULL == type->data) return;

	network_mysqld_type_float_free(type->data);
}

static int network_mysqld_type_data_float_get_double(network_mysqld_type_t *type, double *dst) {
	network_mysqld_type_float_t *src = type->data;

	if (NULL == type->data) return -1;

	*dst = (double)*src;

	return 0;
}

static int network_mysqld_type_data_float_set_double(network_mysqld_type_t *type, double src) {
	network_mysqld_type_float_t *dst = type->data;

	if (NULL == type->data) {
		type->data = network_mysqld_type_float_new();
	}

	dst = type->data;
	*dst = (float)src;

	return 0;
}

static void network_mysqld_type_data_float_init(network_mysqld_type_t *type, enum enum_field_types field_type) {
	type->type	= field_type;
	type->free_data = network_mysqld_type_data_float_free;
	type->get_double = network_mysqld_type_data_float_get_double;
	type->set_double = network_mysqld_type_data_float_set_double;
}

/* MYSQL_TYPE_STRING */
static network_mysqld_type_string_t *network_mysqld_type_string_new(void) {
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

static int network_mysqld_type_data_string_get_string_const(network_mysqld_type_t *type, const char **dst, gsize *dst_len) {
	GString *src = type->data;

	if (NULL == type->data) return -1;

	*dst = src->str;
	*dst_len = src->len;
	
	return 0;
}

static int network_mysqld_type_data_string_set_string(network_mysqld_type_t *type, const char *src, gsize src_len) {
	GString *dst;

	if (NULL == type->data) {
		type->data = g_string_sized_new(src_len);
	}

	dst = type->data;

	g_string_assign_len(dst, src, src_len);
	
	return 0;
}

static void network_mysqld_type_data_string_init(network_mysqld_type_t *type, enum enum_field_types field_type) {
	type->type	= field_type;
	type->free_data = network_mysqld_type_data_string_free;
	type->get_string_const = network_mysqld_type_data_string_get_string_const;
	type->set_string = network_mysqld_type_data_string_set_string;
}

/* MYSQL_TYPE_DATE */
static network_mysqld_type_date_t *network_mysqld_type_date_new(void) {
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

static int network_mysqld_type_data_date_get_date(network_mysqld_type_t *type, network_mysqld_type_date_t *dst) {
	network_mysqld_type_date_t *src = type->data;

	if (NULL == type->data) return -1;

	memcpy(dst, src, sizeof(network_mysqld_type_date_t));

	return 0;
}

static int network_mysqld_type_data_date_set_date(network_mysqld_type_t *type, network_mysqld_type_date_t *src) {
	network_mysqld_type_date_t *dst;

	if (NULL == type->data) {
		type->data = network_mysqld_type_date_new();
	}

	dst = type->data;

	memcpy(dst, src, sizeof(network_mysqld_type_date_t));

	return 0;
}

static void network_mysqld_type_data_date_init(network_mysqld_type_t *type, enum enum_field_types field_type) {
	type->type	= field_type;
	type->free_data = network_mysqld_type_data_date_free;
	type->get_date   = network_mysqld_type_data_date_get_date;
	type->set_date   = network_mysqld_type_data_date_set_date;
}


/* MYSQL_TYPE_TIME */
static network_mysqld_type_time_t *network_mysqld_type_time_new(void) {
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

static int network_mysqld_type_data_time_get_time(network_mysqld_type_t *type, network_mysqld_type_time_t *dst) {
	network_mysqld_type_date_t *src = type->data;

	if (NULL == type->data) return -1;

	memcpy(dst, src, sizeof(network_mysqld_type_time_t));

	return 0;
}

static int network_mysqld_type_data_time_set_time(network_mysqld_type_t *type, network_mysqld_type_time_t *src) {
	network_mysqld_type_date_t *dst;

	if (NULL == type->data) {
		type->data = network_mysqld_type_time_new();
	}
	dst = type->data;

	memcpy(dst, src, sizeof(network_mysqld_type_time_t));

	return 0;
}


static void network_mysqld_type_data_time_init(network_mysqld_type_t *type, enum enum_field_types field_type) {
	type->type	= field_type;
	type->free_data = network_mysqld_type_data_time_free;
	type->get_time = network_mysqld_type_data_time_get_time;
	type->set_time = network_mysqld_type_data_time_set_time;
}


/**
 * create a type 
 */
network_mysqld_type_t *network_mysqld_type_new(enum enum_field_types field_type) {
	network_mysqld_type_t *type;

	type = g_slice_new0(network_mysqld_type_t);

	switch (field_type) {
	case MYSQL_TYPE_TINY:
	case MYSQL_TYPE_SHORT:
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_LONGLONG:
		network_mysqld_type_data_int_init(type, field_type);
		break;
	case MYSQL_TYPE_FLOAT: /* 4 bytes */
		network_mysqld_type_data_float_init(type, field_type);
		break;
	case MYSQL_TYPE_DOUBLE: /* 8 bytes */
		network_mysqld_type_data_double_init(type, field_type);
		break;
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_TIMESTAMP:
		network_mysqld_type_data_date_init(type, field_type);
		break;
	case MYSQL_TYPE_TIME:
		network_mysqld_type_data_time_init(type, field_type);
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
		network_mysqld_type_data_string_init(type, field_type);
		break;
	case MYSQL_TYPE_NULL:
		type->type = field_type;
		break;
	default:
		g_error("%s: type = %d isn't known", G_STRLOC, field_type);
		return NULL;
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

/* ints */
static int network_mysqld_type_factory_int_from_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, network_packet *packet, network_mysqld_type_t *type) {
	int err = 0;

	if (type->type == MYSQL_TYPE_TINY) {
		guint8 i8;

		err = err || network_mysqld_proto_get_int8(packet, &i8);
		err = err || type->set_int(type, (guint64)i8, type->is_unsigned);
	} else if (type->type == MYSQL_TYPE_SHORT) {
		guint16 i16;

		err = err || network_mysqld_proto_get_int16(packet, &i16);
		err = err || type->set_int(type, (guint64)i16, type->is_unsigned);
	} else if (type->type == MYSQL_TYPE_LONG || type->type == MYSQL_TYPE_INT24) {
		guint32 i32;

		err = err || network_mysqld_proto_get_int32(packet, &i32);
		err = err || type->set_int(type, (guint64)i32, type->is_unsigned);
	} else {
		guint64 i64;

		err = err || network_mysqld_proto_get_int64(packet, &i64);
		err = err || type->set_int(type, i64, type->is_unsigned);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_int_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString *packet, network_mysqld_type_t *type) {
	guint64 i64;

	type->get_int(type, &i64, NULL);

	if (type->type == MYSQL_TYPE_TINY) {
		guint8  i8;

		i8 = i64;

		network_mysqld_proto_append_int8(packet, i8);
	} else if (type->type == MYSQL_TYPE_SHORT) {
		guint16  i16;

		i16 = i64;

		network_mysqld_proto_append_int16(packet, i16);
	} else if (type->type == MYSQL_TYPE_LONG || type->type == MYSQL_TYPE_INT24) {
		guint32  i32;

		i32 = i64;

		network_mysqld_proto_append_int32(packet, i32);
	} else if (type->type == MYSQL_TYPE_LONGLONG) {
		network_mysqld_proto_append_int64(packet, i64);
	}

	return 0;
}

/** 
 * create a factory for TINYs 
 */
static network_mysqld_type_factory_t *network_mysqld_type_factory_int_new(void) {
	network_mysqld_type_factory_t *factory;

	factory = g_slice_new0(network_mysqld_type_factory_t);
	factory->to_binary = network_mysqld_type_factory_int_to_binary;
	factory->from_binary = network_mysqld_type_factory_int_from_binary;

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
		type->set_double(type, double_copy.d);
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

static network_mysqld_type_factory_t *network_mysqld_type_factory_double_new() {
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
		err = err || type->set_double(type, (double)float_copy.d);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_factory_float_to_binary(network_mysqld_type_factory_t G_GNUC_UNUSED *factory, GString *packet, network_mysqld_type_t *type) {
	union {
		float f;
		char d_char_shadow[sizeof(float)];
	} float_copy;
	double d;

	type->get_double(type, &d);

	float_copy.f = (float)d;

	g_string_append_len(packet, float_copy.d_char_shadow, sizeof(float));

	return 0;
}

static network_mysqld_type_factory_t *network_mysqld_type_factory_float_new() {
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

static network_mysqld_type_factory_t *network_mysqld_type_factory_string_new(enum enum_field_types field_type) {
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

static network_mysqld_type_factory_t *network_mysqld_type_factory_date_new(enum enum_field_types field_type) {
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

static network_mysqld_type_factory_t *network_mysqld_type_factory_time_new(void) {
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
	case MYSQL_TYPE_SHORT:
	case MYSQL_TYPE_LONG:
	case MYSQL_TYPE_INT24:
	case MYSQL_TYPE_LONGLONG:
		return network_mysqld_type_factory_int_new();
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

