#include <sys/types.h>

/**
 * replication 
 */
#include "glib-ext.h"
#include "network-mysqld-binlog.h"

#define S(x) x->str, x->len

network_mysqld_table *network_mysqld_table_new() {
	network_mysqld_table *tbl;

	tbl = g_new0(network_mysqld_table, 1);
	tbl->db_name = g_string_new(NULL);
	tbl->table_name = g_string_new(NULL);

	tbl->fields = network_mysqld_proto_fielddefs_new();

	return tbl;
}

void network_mysqld_table_free(network_mysqld_table *tbl) {
	if (!tbl) return;

	g_string_free(tbl->db_name, TRUE);
	g_string_free(tbl->table_name, TRUE);

	network_mysqld_proto_fielddefs_free(tbl->fields);

	g_free(tbl);
}

static guint guint64_hash(gconstpointer _key) {
	const guint64 *key = _key;

	return *key & 0xffffffff;
}

static gboolean guint64_equal(gconstpointer _a, gconstpointer _b) {
	const guint64 *a = _a;
	const guint64 *b = _b;

	return *a == *b;
}

guint64 *guint64_new(guint64 i) {
	guint64 *ip;

	ip = g_new0(guint64, 1);
	*ip = i;

	return ip;
}

network_mysqld_binlog *network_mysqld_binlog_new() {
	network_mysqld_binlog *binlog;

	binlog = g_new0(network_mysqld_binlog, 1);
	binlog->rbr_tables = g_hash_table_new_full(
			guint64_hash,
			guint64_equal,
			g_free,
			(GDestroyNotify)network_mysqld_table_free);

	return binlog;
}

void network_mysqld_binlog_free(network_mysqld_binlog *binlog) {
	if (!binlog) return;

	g_hash_table_destroy(binlog->rbr_tables);

	g_free(binlog);
}

network_mysqld_binlog_event *network_mysqld_binlog_event_new() {
	network_mysqld_binlog_event *binlog;

	binlog = g_new0(network_mysqld_binlog_event, 1);

	return binlog;
}

int network_mysqld_proto_get_binlog_status(network_packet *packet) {
	gint8 ok;

	/* on the network we have a length and packet-number of 4 bytes */
	ok = network_mysqld_proto_get_int8(packet);
	g_return_val_if_fail(ok == 0, -1);

	return 0;
}

int network_mysqld_proto_get_binlog_event_header(network_packet *packet, network_mysqld_binlog_event *event) {
	event->timestamp  = network_mysqld_proto_get_int32(packet);
	event->event_type = network_mysqld_proto_get_int8(packet);
	event->server_id  = network_mysqld_proto_get_int32(packet);
	event->event_size = network_mysqld_proto_get_int32(packet);
	event->log_pos    = network_mysqld_proto_get_int32(packet);
	event->flags      = network_mysqld_proto_get_int16(packet);

	return 0;
}

int network_mysqld_proto_get_binlog_event(network_packet *packet, 
		network_mysqld_binlog *binlog,
		network_mysqld_binlog_event *event) {

	network_mysqld_table *tbl;

	switch ((guchar)event->event_type) {
	case QUERY_EVENT:
		event->event.query_event.thread_id   = network_mysqld_proto_get_int32(packet);
		event->event.query_event.exec_time   = network_mysqld_proto_get_int32(packet);
		event->event.query_event.db_name_len = network_mysqld_proto_get_int8(packet);
		event->event.query_event.error_code  = network_mysqld_proto_get_int16(packet);

		/* 5.0 has more flags */
		if (packet->data->len > packet->offset) {
			guint16 var_size = 0;

			var_size    = network_mysqld_proto_get_int16(packet);
			if (var_size) {
				/* skip the variable size part for now */
				network_mysqld_proto_skip(packet, var_size);
			}
	
			/* default db has <db_name_len> chars */
			event->event.query_event.db_name = network_mysqld_proto_get_string_len(packet, 
					event->event.query_event.db_name_len);
			network_mysqld_proto_skip(packet, 1); /* the \0 */
	
			event->event.query_event.query = network_mysqld_proto_get_string_len(packet, 
					packet->data->len - packet->offset);
		}

		break;
	case ROTATE_EVENT:
		event->event.rotate_event.binlog_pos = network_mysqld_proto_get_int32(packet);
		network_mysqld_proto_skip(packet, 4);
		event->event.rotate_event.binlog_file = network_mysqld_proto_get_string_len(
				packet, 
				packet->data->len - packet->offset);
		break;
	case STOP_EVENT:
		/* is empty */
		break;
	case FORMAT_DESCRIPTION_EVENT:
		event->event.format_event.binlog_version = network_mysqld_proto_get_int16(packet);
		event->event.format_event.master_version = network_mysqld_proto_get_string_len( /* NUL-term string */
				packet, ST_SERVER_VER_LEN);
		event->event.format_event.created_ts = network_mysqld_proto_get_int32(packet);

		/* the header length may change in the future, for now we assume it is 19 */
		event->event.format_event.log_header_len = network_mysqld_proto_get_int8(packet);
		g_assert_cmpint(event->event.format_event.log_header_len, ==, 19);

		/* there is some funky event-permutation going on */
		event->event.format_event.perm_events_len = packet->data->len - packet->offset;
		event->event.format_event.perm_events = network_mysqld_proto_get_string_len(
				packet, packet->data->len - packet->offset);
		
		break;
	case USER_VAR_EVENT:
		event->event.user_var_event.name_len = network_mysqld_proto_get_int32(packet);
		event->event.user_var_event.name = network_mysqld_proto_get_string_len(
				packet,
				event->event.user_var_event.name_len);

		event->event.user_var_event.is_null = network_mysqld_proto_get_int8(packet);
		event->event.user_var_event.type = network_mysqld_proto_get_int8(packet);
		event->event.user_var_event.charset = network_mysqld_proto_get_int32(packet);
		event->event.user_var_event.value_len = network_mysqld_proto_get_int32(packet);
		event->event.user_var_event.value = network_mysqld_proto_get_string_len(
				packet,
				event->event.user_var_event.value_len);
		break;
	case TABLE_MAP_EVENT: /* 19 */
		/**
		 * looks like a abstract definition of a table 
		 *
		 * no, we don't want to know
		 */
		event->event.table_map_event.table_id = network_mysqld_proto_get_int48(packet); /* 6 bytes */
		event->event.table_map_event.flags = network_mysqld_proto_get_int16(packet);

		event->event.table_map_event.db_name_len = network_mysqld_proto_get_int8(packet);
		event->event.table_map_event.db_name = network_mysqld_proto_get_string_len(
				packet,
				event->event.table_map_event.db_name_len);
		network_mysqld_proto_skip(packet, 1); /* this should be NUL */

		event->event.table_map_event.table_name_len = network_mysqld_proto_get_int8(packet);
		event->event.table_map_event.table_name = network_mysqld_proto_get_string_len(
				packet,
				event->event.table_map_event.table_name_len);
		network_mysqld_proto_skip(packet, 1); /* this should be NUL */

		event->event.table_map_event.columns_len = network_mysqld_proto_get_lenenc_int(packet);
		event->event.table_map_event.columns = network_mysqld_proto_get_string_len(
				packet,
				event->event.table_map_event.columns_len);

		event->event.table_map_event.metadata_len = network_mysqld_proto_get_lenenc_int(packet);
		event->event.table_map_event.metadata = network_mysqld_proto_get_string_len(
				packet,
				event->event.table_map_event.metadata_len);

		/**
		 * the null-bit count is columns/8 
		 */

		event->event.table_map_event.null_bits_len = (int)((event->event.table_map_event.columns_len+7)/8);
		event->event.table_map_event.null_bits = network_mysqld_proto_get_string_len(
				packet,
				event->event.table_map_event.null_bits_len);

		g_assert_cmpint(packet->data->len, ==, packet->offset); /* this should be the full packet */
		break;
	case DELETE_ROWS_EVENT: /* 25 */
	case UPDATE_ROWS_EVENT: /* 24 */
	case WRITE_ROWS_EVENT:  /* 23 */
		event->event.row_event.table_id = network_mysqld_proto_get_int48(packet); /* 6 bytes */
		event->event.row_event.flags = network_mysqld_proto_get_int16(packet);
		
		event->event.row_event.columns_len = network_mysqld_proto_get_lenenc_int(packet);

		/* a bit-mask of used-fields (m_cols.bitmap) */
		event->event.row_event.used_columns_len = (int)((event->event.row_event.columns_len+7)/8);
		event->event.row_event.used_columns = network_mysqld_proto_get_string_len(
				packet,
				event->event.row_event.used_columns_len);

		if (event->event_type == UPDATE_ROWS_EVENT) {
			/* the before image */
			network_mysqld_proto_skip(packet, event->event.row_event.used_columns_len);
		}

		/* null-bits for all the columns */
		event->event.row_event.null_bits_len = (int)((event->event.row_event.columns_len+7)/8);

		/* the null-bits + row,
		 *
		 * the rows are stored in field-format, to decode we have to see
		 * the table description
		 */
		event->event.row_event.row_len = packet->data->len - packet->offset;
		event->event.row_event.row = network_mysqld_proto_get_string_len(
				packet,
				event->event.row_event.row_len);
		
		break;
	case INTVAR_EVENT:
		event->event.intvar.type = network_mysqld_proto_get_int8(packet);
		event->event.intvar.value = network_mysqld_proto_get_int64(packet);

		break;
	case XID_EVENT:
		event->event.xid.xid_id = network_mysqld_proto_get_int64(packet);
		break;
	default:
		g_critical("%s: unhandled binlog-event: %d", G_STRLOC, event->event_type);
		return -1;
	}

	/* we should check if we have handled all bytes */

	return 0;
}

void network_mysqld_binlog_event_free(network_mysqld_binlog_event *event) {
	if (!event) return;

	switch (event->event_type) {
	case QUERY_EVENT:
		if (event->event.query_event.db_name) g_free(event->event.query_event.db_name);
		if (event->event.query_event.query) g_free(event->event.query_event.query);
		break;
	case ROTATE_EVENT:
		if (event->event.rotate_event.binlog_file) g_free(event->event.rotate_event.binlog_file);
		break;
	case FORMAT_DESCRIPTION_EVENT:
		if (event->event.format_event.master_version) g_free(event->event.format_event.master_version);
		if (event->event.format_event.perm_events) g_free(event->event.format_event.perm_events);
		break;
	case USER_VAR_EVENT:
		if (event->event.user_var_event.name) g_free(event->event.user_var_event.name);
		if (event->event.user_var_event.value) g_free(event->event.user_var_event.value);
		break;
	case TABLE_MAP_EVENT:
		if (event->event.table_map_event.db_name) g_free(event->event.table_map_event.db_name);
		if (event->event.table_map_event.table_name) g_free(event->event.table_map_event.table_name);
		if (event->event.table_map_event.columns) g_free(event->event.table_map_event.columns);
		if (event->event.table_map_event.metadata) g_free(event->event.table_map_event.metadata);
		if (event->event.table_map_event.null_bits) g_free(event->event.table_map_event.null_bits);
		break;
	case DELETE_ROWS_EVENT:
	case UPDATE_ROWS_EVENT:
	case WRITE_ROWS_EVENT:
		if (event->event.row_event.used_columns) g_free(event->event.row_event.used_columns);
		if (event->event.row_event.row) g_free(event->event.row_event.row);
		break;
	default:
		break;
	}

	g_free(event);
}


network_mysqld_binlog_dump *network_mysqld_binlog_dump_new() {
	network_mysqld_binlog_dump *dump;

	dump = g_new0(network_mysqld_binlog_dump, 1);

	return dump;
}

int network_mysqld_proto_append_binlog_dump(GString *packet, network_mysqld_binlog_dump *dump) {
	network_mysqld_proto_append_int8(packet, COM_BINLOG_DUMP);
	network_mysqld_proto_append_int32(packet, dump->binlog_pos);
	network_mysqld_proto_append_int16(packet, dump->flags); /* flags */
	network_mysqld_proto_append_int32(packet, dump->server_id);
	g_string_append(packet, dump->binlog_file); /* filename */
	network_mysqld_proto_append_int8(packet, 0); /* term-nul */

	return 0;
}

void network_mysqld_binlog_dump_free(network_mysqld_binlog_dump *dump) {
	if (!dump) return;

	g_free(dump);
}


/**
 * decode the table-map event
 *
 * 
 */
int network_mysqld_binlog_event_tablemap_get(
		network_mysqld_binlog_event *event,
		network_mysqld_table *tbl) {

	network_packet metadata_packet;
	GString row;
	guint i;

	g_string_assign(tbl->db_name, event->event.table_map_event.db_name);
	g_string_assign(tbl->table_name, event->event.table_map_event.table_name);

	tbl->table_id = event->event.table_map_event.table_id;

	row.str = event->event.table_map_event.metadata;
	row.len = event->event.table_map_event.metadata_len;

	metadata_packet.data = &row;
	metadata_packet.offset = 0;

	/* the metadata is field specific */
	for (i = 0; i < event->event.table_map_event.columns_len; i++) {
		MYSQL_FIELD *field = network_mysqld_proto_fielddef_new();
		enum enum_field_types col_type;

		guint byteoffset = i / 8;
		guint bitoffset = i % 8;

		field->flags |= (event->event.table_map_event.null_bits[byteoffset] >> bitoffset) & 0x1 ? 0 : NOT_NULL_FLAG;

		col_type = (enum enum_field_types)event->event.table_map_event.columns[i];

		/* the meta-data depends on the type,
		 *
		 * string has 2 byte field-length
		 * floats have precision
		 * ints have display length
		 * */
		switch ((guchar)col_type) {
		case MYSQL_TYPE_STRING: /* 254 (CHAR) */
			/* byte 0: real_type 
			 * byte 1: field-length
			 */
			field->type  = network_mysqld_proto_get_int8(&metadata_packet);
			field->max_length = network_mysqld_proto_get_int8(&metadata_packet);

			break;
		case MYSQL_TYPE_VARCHAR: /* 15 (VARCHAR) */
		case MYSQL_TYPE_VAR_STRING:
			/* 2 byte length (int2store)
			 */
			field->type = col_type;
			field->max_length = network_mysqld_proto_get_int16(&metadata_packet);
			break;
		case MYSQL_TYPE_BLOB: /* 252 */
			field->type = col_type;

			/* the packlength (1 .. 4) */
			field->max_length = network_mysqld_proto_get_int8(&metadata_packet);
			break;
		case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_DECIMAL:
			field->type = col_type;
			/**
			 * byte 0: precisions
			 * byte 1: decimals
			 */
			field->max_length = network_mysqld_proto_get_int8(&metadata_packet);
			field->decimals = network_mysqld_proto_get_int8(&metadata_packet);
			break;
		case MYSQL_TYPE_DOUBLE:
		case MYSQL_TYPE_FLOAT:
			field->type = col_type;
			/* pack-length */
			field->max_length = network_mysqld_proto_get_int8(&metadata_packet);
			break;
		case MYSQL_TYPE_ENUM:
			/* real-type (ENUM|SET)
			 * pack-length
			 */
			field->type  = network_mysqld_proto_get_int8(&metadata_packet);
			field->max_length = network_mysqld_proto_get_int8(&metadata_packet);
			break;
		case MYSQL_TYPE_BIT:
			network_mysqld_proto_skip(&metadata_packet, 2);
			break;
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:

		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONG:
			field->type = col_type;
			break;
		default:
			g_error("%s: field-type %d isn't handled",
					G_STRLOC,
					col_type
					);
			break;
		}

		g_ptr_array_add(tbl->fields, field);
	}

	if (metadata_packet.offset != metadata_packet.data->len) {
		g_debug_hexdump(G_STRLOC, event->event.table_map_event.columns, event->event.table_map_event.columns_len);
		g_debug_hexdump(G_STRLOC, event->event.table_map_event.metadata, event->event.table_map_event.metadata_len);
	}
	g_assert_cmpint(metadata_packet.offset, ==, metadata_packet.data->len);

	return 0;
}


