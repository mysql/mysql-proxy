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

#include <string.h>
#include "network-mysqld-proto.h"

#include "sys-pedantic.h"

#define CRASHME() do { char *_crashme = NULL; *_crashme = 0; } while(0);
/**
 * decode a length-encoded integer
 */
guint64 network_mysqld_proto_decode_lenenc(GString *s, guint *_off) {
	guint off = *_off;
	guint64 ret = 0;
	unsigned char *bytestream = (unsigned char *)s->str;

	g_assert(off < s->len);
	
	if (bytestream[off] < 251) { /* */
		ret = bytestream[off];
	} else if (bytestream[off] == 251) { /* NULL in row-data */
		ret = bytestream[off];
	} else if (bytestream[off] == 252) { /* 2 byte length*/
		g_assert(off + 2 < s->len);
		ret = (bytestream[off + 1] << 0) | 
			(bytestream[off + 2] << 8) ;
		off += 2;
	} else if (bytestream[off] == 253) { /* 3 byte */
		g_assert(off + 3 < s->len);
		ret = (bytestream[off + 1]   <<  0) | 
			(bytestream[off + 2] <<  8) |
			(bytestream[off + 3] << 16);

		off += 3;
	} else if (bytestream[off] == 254) { /* 8 byte */
		g_assert(off + 8 < s->len);
		ret = (bytestream[off + 5] << 0) |
			(bytestream[off + 6] << 8) |
			(bytestream[off + 7] << 16) |
			(bytestream[off + 8] << 24);
		ret <<= 32;

		ret |= (bytestream[off + 1] <<  0) | 
			(bytestream[off + 2] <<  8) |
			(bytestream[off + 3] << 16) |
			(bytestream[off + 4] << 24);
		

		off += 8;
	} else {
		g_error("%s.%d: bytestream[%d] is %d", 
			__FILE__, __LINE__,
			off, bytestream[off]);
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
	if (*_off + size > packet->len) {
		CRASHME();
	}
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

gchar *network_mysqld_proto_get_lenenc_string(GString *packet, guint *_off) {
	guint64 len;

	len = network_mysqld_proto_decode_lenenc(packet, _off);
	
	g_assert(*_off < packet->len);
	g_assert(*_off + len <= packet->len);
	
	return network_mysqld_proto_get_string_len(packet, _off, len);
}

gchar *network_mysqld_proto_get_string(GString *packet, guint *_off) {
	guint len;
	gchar *r = NULL;

	for (len = 0; *_off + len < packet->len && *(packet->str + *_off + len); len++);

	g_assert(*(packet->str + *_off + len) == '\0'); /* this has to be a \0 */

	if (len > 0) {
		g_assert(*_off < packet->len);
		g_assert(*_off + len <= packet->len);

		/**
		 * copy the string w/o the NUL byte 
		 */
		r = network_mysqld_proto_get_string_len(packet, _off, len);
	}

	*_off += 1;

	return r;
}


/**
 * copy a len bytes from the packet into the out 
 *
 * increments _off by len
 *
 * @param out a GString which cares the string
 * @return a pointer to the string in out
 */
gchar *network_mysqld_proto_get_gstring_len(GString *packet, guint *_off, gsize len, GString *out) {
	g_string_truncate(out, 0);

	if (len) {
		g_assert(*_off < packet->len);
		if (*_off + len > packet->len) {
			g_error("packet-offset out of range: %u + "F_SIZE_T" > "F_SIZE_T, *_off, len, packet->len);
		}

		g_string_append_len(out, packet->str + *_off, len);
		*_off += len;
	}

	return out->str;
}

gchar *network_mysqld_proto_get_gstring(GString *packet, guint *_off, GString *out) {
	guint len;
	gchar *r = NULL;

	for (len = 0; *_off + len < packet->len && *(packet->str + *_off + len); len++);

	g_assert(*(packet->str + *_off + len) == '\0'); /* this has to be a \0 */

	if (len > 0) {
		g_assert(*_off < packet->len);
		g_assert(*_off + len <= packet->len);

		r = network_mysqld_proto_get_gstring_len(packet, _off, len, out);
	}

	/* skip the \0 */
	*_off += 1;

	return r;
}

gchar *network_mysqld_proto_get_lenenc_gstring(GString *packet, guint *_off, GString *out) {
	guint64 len;

	len = network_mysqld_proto_decode_lenenc(packet, _off);

	return network_mysqld_proto_get_gstring_len(packet, _off, len, out);
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

int network_mysqld_proto_set_header(unsigned char *header, size_t len, unsigned char id) {
	g_assert(len <= PACKET_LEN_MAX);

	header[0] = (len >>  0) & 0xFF;
	header[1] = (len >>  8) & 0xFF;
	header[2] = (len >> 16) & 0xFF;
	header[3] = id;

	return 0;
}

size_t network_mysqld_proto_get_header(unsigned char *header) {
	return header[0] | header[1] << 8 | header[2] << 16;
}

int network_mysqld_proto_append_lenenc_int(GString *dest, guint64 len) {
	if (len < 251) {
		g_string_append_c(dest, len);
	} else if (len < 65536) {
		g_string_append_c(dest, (gchar)252);
		g_string_append_c(dest, (len >> 0) & 0xff);
		g_string_append_c(dest, (len >> 8) & 0xff);
	} else if (len < 16777216) {
		g_string_append_c(dest, (gchar)253);
		g_string_append_c(dest, (len >> 0) & 0xff);
		g_string_append_c(dest, (len >> 8) & 0xff);
		g_string_append_c(dest, (len >> 16) & 0xff);
	} else {
		g_string_append_c(dest, (gchar)254);

		g_string_append_c(dest, (len >> 0) & 0xff);
		g_string_append_c(dest, (len >> 8) & 0xff);
		g_string_append_c(dest, (len >> 16) & 0xff);
		g_string_append_c(dest, (len >> 24) & 0xff);

		g_string_append_c(dest, (len >> 32) & 0xff);
		g_string_append_c(dest, (len >> 40) & 0xff);
		g_string_append_c(dest, (len >> 48) & 0xff);
		g_string_append_c(dest, (len >> 56) & 0xff);
	}

	return 0;
}

/**
 * encode a GString in to a MySQL len-encoded string 
 *
 * @param destination string
 * @param string to encode
 * @param length of the string
 */
int network_mysqld_proto_append_lenenc_string_len(GString *dest, const char *s, guint64 len) {
	if (!s) {
		g_string_append_c(dest, (gchar)251); /** this is NULL */
	} else {
		network_mysqld_proto_append_lenenc_int(dest, len);
		g_string_append_len(dest, s, len);
	}

	return 0;
}

/**
 * encode a GString in to a MySQL len-encoded string 
 *
 * @param destination string
 * @param string to encode
 */
int network_mysqld_proto_append_lenenc_string(GString *dest, const char *s) {
	return network_mysqld_proto_append_lenenc_string_len(dest, s, s ? strlen(s) : 0);
}

static int network_mysqld_proto_append_int_len(GString *packet, guint64 num, gsize size) {
	gsize i;

	for (i = 0; i < size; i++) {
		g_string_append_c(packet, num & 0xff);
		num >>= 8;
	}

	return 0;
}

int network_mysqld_proto_append_int8(GString *packet, guint8 num) {
	return network_mysqld_proto_append_int_len(packet, num, sizeof(num));
}

int network_mysqld_proto_append_int16(GString *packet, guint16 num) {
	return network_mysqld_proto_append_int_len(packet, num, sizeof(num));
}

int network_mysqld_proto_append_int32(GString *packet, guint32 num) {
	return network_mysqld_proto_append_int_len(packet, num, sizeof(num));
}


