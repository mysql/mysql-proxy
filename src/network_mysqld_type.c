#include "network_mysqld_type.h"

/* tiny */
static int network_mysqld_type_tiny_from_binary(network_mysqld_type_t *type, network_packet *packet) {
	guint8 i8;
	int err;

	err = err || network_mysqld_proto_get_int8(packet, &i8);

	if (0 == err) {
		type->data = GINT_TO_POINTER(i8);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_tiny_to_binary(network_mysqld_type_t *type, GString *packet) {
	gint i = GPOINTER_TO_INT(type->data);

	network_mysqld_proto_append_int8(packet, i);

	return 0;
}

network_mysqld_type_t *network_mysqld_type_tiny_new() {
	network_mysqld_type_t *type;

	type = g_slice_new0(network_mysqld_type_t);
	type->to_binary = network_mysqld_type_tiny_to_binary;
	type->from_binary = network_mysqld_type_tiny_from_binary;
	type->type = MYSQL_TYPE_TINY;

	return type;
}

/* short */
static int network_mysqld_type_short_from_binary(network_mysqld_type_t *type, network_packet *packet) {
	guint16 i16;
	int err;

	err = err || network_mysqld_proto_get_int16(packet, &i16);

	if (0 == err) {
		type->data = GINT_TO_POINTER(i16);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_short_to_binary(network_mysqld_type_t *type, GString *packet) {
	gint i = GPOINTER_TO_INT(type->data);

	network_mysqld_proto_append_int16(packet, i);

	return 0;
}

network_mysqld_type_t *network_mysqld_type_short_new() {
	network_mysqld_type_t *type;

	type = g_slice_new0(network_mysqld_type_t);
	type->to_binary = network_mysqld_type_short_to_binary;
	type->from_binary = network_mysqld_type_short_from_binary;
	type->type = MYSQL_TYPE_SHORT;

	return type;
}

/* long */
static int network_mysqld_type_long_from_binary(network_mysqld_type_t *type, network_packet *packet) {
	guint32 i32;
	int err;

	err = err || network_mysqld_proto_get_int32(packet, &i32);

	if (0 == err) {
		type->data = GINT_TO_POINTER(i32);
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_long_to_binary(network_mysqld_type_t *type, GString *packet) {
	gint i = GPOINTER_TO_INT(type->data);

	network_mysqld_proto_append_int32(packet, i);

	return 0;
}

network_mysqld_type_t *network_mysqld_type_long_new() {
	network_mysqld_type_t *type;

	type = g_slice_new0(network_mysqld_type_t);
	type->to_binary = network_mysqld_type_long_to_binary;
	type->from_binary = network_mysqld_type_long_from_binary;
	type->type = MYSQL_TYPE_SHORT;

	return type;
}

/* longlong */
static int network_mysqld_type_longlong_from_binary(network_mysqld_type_t *type, network_packet *packet) {
	guint64 i64;
	int err;

	err = err || network_mysqld_proto_get_int64(packet, &i64);

	if (0 == err) {
		type->data = g_slice_new(guint64);
		*(guint64 *)(type->data) = i64;
	}

	return err ? -1 : 0;
}

static int network_mysqld_type_longlong_to_binary(network_mysqld_type_t *type, GString *packet) {
	guint64 i64;

	i64 = *(guint64 *)(type->data); 

	network_mysqld_proto_append_int64(packet, i64);

	return 0;
}

static void network_mysqld_type_longlong_free(network_mysqld_type_t *type) {
	if (NULL == type) return;

	if (NULL == type->data) return;

	g_slice_free(guint64, type->data);
}

network_mysqld_type_t *network_mysqld_type_longlong_new() {
	network_mysqld_type_t *type;

	type = g_slice_new0(network_mysqld_type_t);
	type->type = MYSQL_TYPE_LONGLONG;
	type->free_data = network_mysqld_type_longlong_free;

	type->to_binary = network_mysqld_type_longlong_to_binary;
	type->from_binary = network_mysqld_type_longlong_from_binary;

	return type;
}


network_mysqld_type_t *network_mysqld_type_new(enum enum_field_types type) {
	switch (type) {
	case MYSQL_TYPE_TINY:
		return network_mysqld_type_tiny_new();
	case MYSQL_TYPE_SHORT:
		return network_mysqld_type_short_new();
	case MYSQL_TYPE_LONG:
		return network_mysqld_type_long_new();
	case MYSQL_TYPE_LONGLONG:
		return network_mysqld_type_longlong_new();
	case MYSQL_TYPE_FLOAT: /* 4 bytes */
	case MYSQL_TYPE_DOUBLE: /* 8 bytes */
	case MYSQL_TYPE_DECIMAL:
	case MYSQL_TYPE_NEWDECIMAL:
	case MYSQL_TYPE_TIME:
	case MYSQL_TYPE_DATE:
	case MYSQL_TYPE_DATETIME:
	case MYSQL_TYPE_BLOB:
	case MYSQL_TYPE_TINY_BLOB:
	case MYSQL_TYPE_MEDIUM_BLOB:
	case MYSQL_TYPE_LONG_BLOB:
	default:
		g_assert_not_reached();
		break;
	}
	return NULL;
}

void network_mysqld_type_free(network_mysqld_type_t *type) {
	if (NULL == type) return;

	if (NULL != type->free_data) {
		type->free_data(type);
	}
	g_slice_free(network_mysqld_type_t, type);
}

