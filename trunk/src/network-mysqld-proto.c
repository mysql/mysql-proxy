/* Copyright (C) 2007, 2008 MySQL AB

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

/** @file
 *
 * decoders and encoders for the MySQL packets
 *
 * - basic data-types
 *   - fixed length integers
 *   - variable length integers
 *   - variable length strings
 * - packet types
 *   - OK packets
 *   - EOF packets
 *   - ERR packets
 *
 */

/**
 * force a crash for gdb and valgrind to get a stacktrace
 */
#define CRASHME() do { char *_crashme = NULL; *_crashme = 0; } while(0);

/**
 * a handy marco for constant strings 
 */
#define C(x) x, sizeof(x) - 1

/** @defgroup proto MySQL Protocol
 * 
 * decoders and encoders for the MySQL packets as described in 
 * http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol
 *
 * */
/*@{*/

/**
 * decode a length-encoded integer from a network packet
 *
 * _off is incremented on success 
 *
 * @param packet   the MySQL-packet to decode
 * @param _off     offset in into the packet 
 * @return the decoded number
 *
 */
guint64 network_mysqld_proto_get_lenenc_int(GString *packet, guint *_off) {
	guint off = *_off;
	guint64 ret = 0;
	unsigned char *bytestream = (unsigned char *)packet->str;

	g_assert(off < packet->len);
	
	if (bytestream[off] < 251) { /* */
		ret = bytestream[off];
	} else if (bytestream[off] == 251) { /* NULL in row-data */
		ret = bytestream[off];
	} else if (bytestream[off] == 252) { /* 2 byte length*/
		g_assert(off + 2 < packet->len);
		ret = (bytestream[off + 1] << 0) | 
			(bytestream[off + 2] << 8) ;
		off += 2;
	} else if (bytestream[off] == 253) { /* 3 byte */
		g_assert(off + 3 < packet->len);
		ret = (bytestream[off + 1]   <<  0) | 
			(bytestream[off + 2] <<  8) |
			(bytestream[off + 3] << 16);

		off += 3;
	} else if (bytestream[off] == 254) { /* 8 byte */
		g_assert(off + 8 < packet->len);
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

/**
 * decode a OK packet from the network packet
 */
int network_mysqld_proto_get_ok_packet(GString *packet, guint64 *affected, guint64 *insert_id, int *server_status, int *warning_count, char **msg) {
	guint off = 0;
	guint64 dest;
	guint field_count;

	field_count = network_mysqld_proto_get_int8(packet, &off);
	g_assert(field_count == 0);

	dest = network_mysqld_proto_get_lenenc_int(packet, &off); if (affected) *affected = dest;
	dest = network_mysqld_proto_get_lenenc_int(packet, &off); if (insert_id) *insert_id = dest;

	dest = network_mysqld_proto_get_int16(packet, &off);      if (server_status) *server_status = dest;
	dest = network_mysqld_proto_get_int16(packet, &off);      if (warning_count) *warning_count = dest;

	if (msg) *msg = NULL;

	return 0;
}

int network_mysqld_proto_append_ok_packet(GString *packet, guint64 affected_rows, guint64 insert_id, guint16 server_status, guint16 warnings) {
	network_mysqld_proto_append_int8(packet, 0); /* no fields */
	network_mysqld_proto_append_lenenc_int(packet, affected_rows);
	network_mysqld_proto_append_lenenc_int(packet, insert_id);
	network_mysqld_proto_append_int16(packet, server_status); /* autocommit */
	network_mysqld_proto_append_int16(packet, warnings); /* no warnings */

	return 0;
}

/**
 * create a ERR packet
 *
 * @note the sqlstate has to match the SQL standard. If no matching SQL state is known, leave it at NULL
 *
 * @param packet      network packet
 * @param errmsg      the error message
 * @param errmsg_len  byte-len of the error-message
 * @param errorcode   mysql error-code we want to send
 * @param sqlstate    if none-NULL, 5-char SQL state to send, if NULL, default SQL state is used
 *
 * @return 0 on success
 */
int network_mysqld_proto_append_error_packet(GString *packet, const char *errmsg, gsize errmsg_len, guint errorcode, const gchar *sqlstate) {
	network_mysqld_proto_append_int8(packet, 0xff); /* ERR */
	network_mysqld_proto_append_int16(packet, errorcode); /* errorcode */
	g_string_append_c(packet, '#');
	if (!sqlstate) {
		g_string_append_len(packet, C("07000"));
	} else {
		g_string_append_len(packet, sqlstate, 5);
	}

	if (errmsg_len < 512) {
		g_string_append_len(packet, errmsg, errmsg_len);
	} else {
		/* truncate the err-msg */
		g_string_append_len(packet, errmsg, 512);
	}

	return 0;
}


/**
 * skip bytes in the network packet
 *
 * a assertion makes sure that we can't skip over the end of the packet 
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @param size   bytes to skip
 *
 */
void network_mysqld_proto_skip(GString *packet, guint *_off, gsize size) {
	g_assert(*_off + size <= packet->len);
	
	*_off += size;
}

/**
 * get a fixed-length integer from the network packet 
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @param size   byte-len of the integer to decode
 * @return a the decoded integer
 */
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

/**
 * get a 8-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint8 network_mysqld_proto_get_int8(GString *packet, guint *_off) {
	return network_mysqld_proto_get_int_len(packet, _off, 1);
}

/**
 * get a 16-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint16 network_mysqld_proto_get_int16(GString *packet, guint *_off) {
	return network_mysqld_proto_get_int_len(packet, _off, 2);
}

/**
 * get a 32-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint32 network_mysqld_proto_get_int32(GString *packet, guint *_off) {
	return network_mysqld_proto_get_int_len(packet, _off, 4);
}

/**
 * get a string from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @param len    length of the string
 * @return the string (allocated) or NULL of len is 0
 */
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

/**
 * get a variable-length string from the network packet
 *
 * variable length strings are prefixed with variable-length integer defining the length of the string
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return the string
 * @see network_mysqld_proto_get_string_len(), network_mysqld_proto_get_lenenc_int()
 */
gchar *network_mysqld_proto_get_lenenc_string(GString *packet, guint *_off) {
	guint64 len;

	len = network_mysqld_proto_get_lenenc_int(packet, _off);
	
	g_assert(*_off < packet->len);
	g_assert(*_off + len <= packet->len);
	
	return network_mysqld_proto_get_string_len(packet, _off, len);
}

/**
 * get a NUL-terminated string from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return       the string
 * @see network_mysqld_proto_get_string_len()
 */
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
 * get a GString from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @param len    bytes to copy
 * @param out    a GString which carries the string
 * @return       a pointer to the string in out
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

/**
 * get a NUL-terminated GString from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @param out    a GString which carries the string
 * @return       a pointer to the string in out
 *
 * @see network_mysqld_proto_get_gstring_len()
 */
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

/**
 * get a variable-length GString from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @param out    a GString which carries the string
 * @return       a pointer to the string in out
 *
 * @see network_mysqld_proto_get_gstring_len(), network_mysqld_proto_get_lenenc_int()
 */
gchar *network_mysqld_proto_get_lenenc_gstring(GString *packet, guint *_off, GString *out) {
	guint64 len;

	len = network_mysqld_proto_get_lenenc_int(packet, _off);

	return network_mysqld_proto_get_gstring_len(packet, _off, len, out);
}

/**
 * create a empty field for a result-set definition
 *
 * @return a empty MYSQL_FIELD
 */
MYSQL_FIELD *network_mysqld_proto_field_init() {
	MYSQL_FIELD *field;
	
	field = g_new0(MYSQL_FIELD, 1);

	return field;
}

/**
 * free a MYSQL_FIELD and its components
 *
 * @param field  the MYSQL_FIELD to free
 */
void network_mysqld_proto_field_free(MYSQL_FIELD *field) {
	if (field->catalog) g_free(field->catalog);
	if (field->db) g_free(field->db);
	if (field->name) g_free(field->name);
	if (field->org_name) g_free(field->org_name);
	if (field->table) g_free(field->table);
	if (field->org_table) g_free(field->org_table);

	g_free(field);
}

/**
 * create a array of MYSQL_FIELD 
 *
 * @return a empty array of MYSQL_FIELD
 */
GPtrArray *network_mysqld_proto_fields_init(void) {
	GPtrArray *fields;
	
	fields = g_ptr_array_new();

	return fields;
}

/**
 * free a array of MYSQL_FIELD 
 *
 * @param fields  array of MYSQL_FIELD to free
 * @see network_mysqld_proto_field_free()
 */
void network_mysqld_proto_fields_free(GPtrArray *fields) {
	guint i;

	for (i = 0; i < fields->len; i++) {
		MYSQL_FIELD *field = fields->pdata[i];

		if (field) network_mysqld_proto_field_free(field);
	}

	g_ptr_array_free(fields, TRUE);
}

/**
 * set length of the packet in the packet header
 *
 * each MySQL packet is 
 *  - is prefixed by a 4 byte packet header
 *  - length is max 16Mbyte (3 Byte)
 *  - sequence-id (1 Byte) 
 *
 * To encode a packet of more then 16M clients have to send multiple 16M frames
 *
 * the sequence-id is incremented for each related packet and wrapping from 255 to 0
 *
 * @param header  string of at least 4 byte to write the packet header to
 * @param length  length of the packet
 * @param id      sequence-id of the packet
 * @return 0
 */
int network_mysqld_proto_set_header(unsigned char *header, size_t length, unsigned char id) {
	g_assert(length <= PACKET_LEN_MAX);

	header[0] = (length >>  0) & 0xFF;
	header[1] = (length >>  8) & 0xFF;
	header[2] = (length >> 16) & 0xFF;
	header[3] = id;

	return 0;
}

/**
 * decode the packet length from a packet header
 *
 * @param header the first 3 bytes of the network packet
 * @return the packet length
 * @see network_mysqld_proto_set_header()
 */
size_t network_mysqld_proto_get_header(unsigned char *header) {
	return header[0] | header[1] << 8 | header[2] << 16;
}

/**
 * append the variable-length integer to the packet
 *
 * @param packet  the MySQL network packet
 * @param length  integer to encode
 * @return        0
 */
int network_mysqld_proto_append_lenenc_int(GString *packet, guint64 length) {
	if (length < 251) {
		g_string_append_c(packet, length);
	} else if (length < 65536) {
		g_string_append_c(packet, (gchar)252);
		g_string_append_c(packet, (length >> 0) & 0xff);
		g_string_append_c(packet, (length >> 8) & 0xff);
	} else if (length < 16777216) {
		g_string_append_c(packet, (gchar)253);
		g_string_append_c(packet, (length >> 0) & 0xff);
		g_string_append_c(packet, (length >> 8) & 0xff);
		g_string_append_c(packet, (length >> 16) & 0xff);
	} else {
		g_string_append_c(packet, (gchar)254);

		g_string_append_c(packet, (length >> 0) & 0xff);
		g_string_append_c(packet, (length >> 8) & 0xff);
		g_string_append_c(packet, (length >> 16) & 0xff);
		g_string_append_c(packet, (length >> 24) & 0xff);

		g_string_append_c(packet, (length >> 32) & 0xff);
		g_string_append_c(packet, (length >> 40) & 0xff);
		g_string_append_c(packet, (length >> 48) & 0xff);
		g_string_append_c(packet, (length >> 56) & 0xff);
	}

	return 0;
}

/**
 * encode a GString in to a MySQL len-encoded string 
 *
 * @param packet  the MySQL network packet
 * @param s       string to encode
 * @param length  length of the string to encode
 * @return 0
 */
int network_mysqld_proto_append_lenenc_string_len(GString *packet, const char *s, guint64 length) {
	if (!s) {
		g_string_append_c(packet, (gchar)251); /** this is NULL */
	} else {
		network_mysqld_proto_append_lenenc_int(packet, length);
		g_string_append_len(packet, s, length);
	}

	return 0;
}

/**
 * encode a GString in to a MySQL len-encoded string 
 *
 * @param packet  the MySQL network packet
 * @param s       string to encode
 *
 * @see network_mysqld_proto_append_lenenc_string_len()
 */
int network_mysqld_proto_append_lenenc_string(GString *packet, const char *s) {
	return network_mysqld_proto_append_lenenc_string_len(packet, s, s ? strlen(s) : 0);
}

/**
 * encode fixed length integer in to a network packet
 *
 * @param packet  the MySQL network packet
 * @param num     integer to encode
 * @param size    byte size of the integer
 * @return        0
 */
static int network_mysqld_proto_append_int_len(GString *packet, guint64 num, gsize size) {
	gsize i;

	for (i = 0; i < size; i++) {
		g_string_append_c(packet, num & 0xff);
		num >>= 8;
	}

	return 0;
}

/**
 * encode 8-bit integer in to a network packet
 *
 * @param packet  the MySQL network packet
 * @param num     integer to encode
 *
 * @see network_mysqld_proto_append_int_len()
 */
int network_mysqld_proto_append_int8(GString *packet, guint8 num) {
	return network_mysqld_proto_append_int_len(packet, num, sizeof(num));
}

/**
 * encode 16-bit integer in to a network packet
 *
 * @param packet  the MySQL network packet
 * @param num     integer to encode
 *
 * @see network_mysqld_proto_append_int_len()
 */
int network_mysqld_proto_append_int16(GString *packet, guint16 num) {
	return network_mysqld_proto_append_int_len(packet, num, sizeof(num));
}

/**
 * encode 32-bit integer in to a network packet
 *
 * @param packet  the MySQL network packet
 * @param num     integer to encode
 *
 * @see network_mysqld_proto_append_int_len()
 */
int network_mysqld_proto_append_int32(GString *packet, guint32 num) {
	return network_mysqld_proto_append_int_len(packet, num, sizeof(num));
}

/*@}*/
