/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */ 

#include "network-mysqld-proto.h"

#include "sys-pedantic.h"

/**
 * decode a length-encoded integer
 */
guint64 network_mysqld_proto_decode_lenenc(GString *s, guint *_off) {
	int off = *_off;
	guint64 ret = 0;
	
	if ((unsigned char)s->str[off] < 251) { /* */
		ret = s->str[off];
	} else if ((unsigned char)s->str[off] == 251) { /* NULL in row-data */
		ret = s->str[off];
	} else if ((unsigned char)s->str[off] == 252) { /* */
		ret = (s->str[off + 1] << 0) | 
			(s->str[off + 2] << 8) ;
		off += 2;
	} else if ((unsigned char)s->str[off] == 253) { /* */
		ret = (s->str[off + 1]   <<  0) | 
			(s->str[off + 2] <<  8) |
			(s->str[off + 3] << 16) |
			(s->str[off + 4] << 24);

		off += 4;
	} else if ((unsigned char)s->str[off] == 254) { /* */
		ret = (s->str[off + 5] << 0) |
			(s->str[off + 6] << 8) |
			(s->str[off + 7] << 16) |
			(s->str[off + 8] << 24);
		ret <<= 32;

		ret |= (s->str[off + 1] <<  0) | 
			(s->str[off + 2] <<  8) |
			(s->str[off + 3] << 16) |
			(s->str[off + 4] << 24);
		

		off += 8;
	} else {
		g_assert(0);
	}
	off += 1;

	*_off = off;

	return ret;
}

int network_mysqld_proto_decode_ok_packet(GString *s, guint64 *affected, guint64 *insert_id, int *server_status, int *warning_count, char **msg) {
	guint off = 0;
	guint64 dest;
	g_assert(s->str[0] == 0);

	off++;

	dest = network_mysqld_proto_decode_lenenc(s, &off); if (affected) *affected = dest;
	dest = network_mysqld_proto_decode_lenenc(s, &off); if (insert_id) *insert_id = dest;

	dest = network_mysqld_proto_get_int16(s, &off);     if (server_status) *server_status = dest;
	dest = network_mysqld_proto_get_int16(s, &off);     if (warning_count) *warning_count = dest;

	if (msg) *msg = NULL;

	return 0;
}


void network_mysqld_proto_skip(GString *packet, guint *_off, gsize size) {
	g_assert(*_off + size <= packet->len);
	
	*_off += size;
}

guint64 network_mysqld_proto_get_int_len(GString *packet, guint *_off, gsize size) {
	gsize i;
	int shift;
	guint64 r = 0;
	guint off = *_off;

	g_assert(*_off < packet->len);
	g_assert(*_off + size <= packet->len);

	for (i = 0, shift = 0; i < size; i++, shift += 8) {
		r += (unsigned char)(packet->str[off + i]) << shift;
	}

	*_off += size;

	return r;
}

guint8 network_mysqld_proto_get_int8(GString *packet, guint *_off) {
	return network_mysqld_proto_get_int_len(packet, _off, 1);
}

guint16 network_mysqld_proto_get_int16(GString *packet, guint *_off) {
	return network_mysqld_proto_get_int_len(packet, _off, 2);
}

guint32 network_mysqld_proto_get_int32(GString *packet, guint *_off) {
	return network_mysqld_proto_get_int_len(packet, _off, 4);
}

gchar *network_mysqld_proto_get_lenenc_string(GString *packet, guint *_off) {
	gchar *str;
	guint64 len;

	len = network_mysqld_proto_decode_lenenc(packet, _off);
	
	g_assert(*_off < packet->len);
	g_assert(*_off + len <= packet->len);

	str = len ? g_strndup(packet->str + *_off, len) : NULL; 

	*_off += len;

	return str;
}

gchar *network_mysqld_proto_get_string(GString *packet, guint *_off) {
	gchar *str;
	guint len;

	for (len = 0; *_off + len < packet->len && *(packet->str + *_off + len); len++);

	g_assert(*(packet->str + *_off + len) == '\0'); /* this has to be a \0 */

	if (len == 0) {
		*_off += 1;

		return NULL;
	}
	
	g_assert(*_off < packet->len);
	g_assert(*_off + len <= packet->len);

	str = len ? g_strndup(packet->str + *_off, len) : NULL; 

	*_off += len + 1;

	return str;
}

gchar *network_mysqld_proto_get_string_len(GString *packet, guint *_off, gsize len) {
	gchar *str;

	g_assert(*_off < packet->len);
	if (*_off + len > packet->len) {
		g_error("packet-offset out of range: %u + "F_SIZE_T" > "F_SIZE_T, *_off, len, packet->len);
	}

	str = len ? g_strndup(packet->str + *_off, len) : NULL; 

	*_off += len;

	return str;
}

MYSQL_FIELD *network_mysqld_proto_field_init() {
	MYSQL_FIELD *field;
	
	field = g_new0(MYSQL_FIELD, 1);

	return field;
}

void network_mysqld_proto_field_free(MYSQL_FIELD *field) {
	if (field->catalog) g_free(field->catalog);
	if (field->db) g_free(field->db);
	if (field->name) g_free(field->name);
	if (field->org_name) g_free(field->org_name);
	if (field->table) g_free(field->table);
	if (field->org_table) g_free(field->org_table);

	g_free(field);
}

GPtrArray *network_mysqld_proto_fields_init(void) {
	GPtrArray *fields;
	
	fields = g_ptr_array_new();

	return fields;
}

void network_mysqld_proto_fields_free(GPtrArray *fields) {
	guint i;

	for (i = 0; i < fields->len; i++) {
		MYSQL_FIELD *field = fields->pdata[i];

		if (field) network_mysqld_proto_field_free(field);
	}

	g_ptr_array_free(fields, TRUE);
}

