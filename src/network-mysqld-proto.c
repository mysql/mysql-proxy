/* $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ */
 

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "network-mysqld-proto.h"

#include "sys-pedantic.h"
#include "glib-ext.h"

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
 * a handy macro for constant strings 
 */
#define C(x) x, sizeof(x) - 1
#define S(x) x->str, x->len

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
 * @param v        destination of the integer
 * @return 0 on success, non-0 on error 
 *
 */
int network_mysqld_proto_get_lenenc_int(network_packet *packet, guint64 *v) {
	guint off = packet->offset;
	guint64 ret = 0;
	unsigned char *bytestream = (unsigned char *)packet->data->str;

	g_return_val_if_fail(off < packet->data->len, -1);
	
	if (bytestream[off] < 251) { /* */
		ret = bytestream[off];
	} else if (bytestream[off] == 251) { /* NULL in row-data */
		ret = bytestream[off];
	} else if (bytestream[off] == 252) { /* 2 byte length*/
		g_return_val_if_fail(off + 2 < packet->data->len, -1);
		ret = (bytestream[off + 1] << 0) | 
			(bytestream[off + 2] << 8) ;
		off += 2;
	} else if (bytestream[off] == 253) { /* 3 byte */
		g_return_val_if_fail(off + 3 < packet->data->len, -1);
		ret = (bytestream[off + 1]   <<  0) | 
			(bytestream[off + 2] <<  8) |
			(bytestream[off + 3] << 16);

		off += 3;
	} else if (bytestream[off] == 254) { /* 8 byte */
		g_return_val_if_fail(off + 8 < packet->data->len, -1);
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
		/* if we hit this place we complete have no idea about the protocol */
		g_critical("%s: bytestream[%d] is %d", 
			G_STRLOC,
			off, bytestream[off]);

		return -1;
	}
	off += 1;

	packet->offset = off;

	*v = ret;

	return 0;
}

/**
 * skip bytes in the network packet
 *
 * a assertion makes sure that we can't skip over the end of the packet 
 *
 * @param packet the MySQL network packet
 * @param size   bytes to skip
 *
 */
int network_mysqld_proto_skip(network_packet *packet, gsize size) {
	if (packet->offset + size > packet->data->len) return -1;
	
	packet->offset += size;

	return 0;
}

/**
 * get a fixed-length integer from the network packet 
 *
 * @param packet the MySQL network packet
 * @param v      destination of the integer
 * @param size   byte-len of the integer to decode
 * @return a the decoded integer
 */
int network_mysqld_proto_peek_int_len(network_packet *packet, guint64 *v, gsize size) {
	gsize i;
	int shift;
	guint32 r_l = 0, r_h = 0;
	guchar *bytes = (guchar *)packet->data->str + packet->offset;

	if (packet->offset > packet->data->len) {
		return -1;
	}
	if (packet->offset + size > packet->data->len) {
		return -1;
	}

	/**
	 * for some reason left-shift > 32 leads to negative numbers 
	 */
	for (i = 0, shift = 0; 
			i < size && i < 4; 
			i++, shift += 8, bytes++) {
		r_l |= ((*bytes) << shift);
	}

	for (shift = 0;
			i < size; 
			i++, shift += 8, bytes++) {
		r_h |= ((*bytes) << shift);
	}

	*v = (((guint64)r_h << 32) | r_l);

	return 0;
}

int network_mysqld_proto_get_int_len(network_packet *packet, guint64 *v, gsize size) {
	int err = 0;

	err = err || network_mysqld_proto_peek_int_len(packet, v, size);

	if (err) return -1;

	packet->offset += size;

	return 0;
}
/**
 * get a 8-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param v      dest for the number
 * @return 0 on success, non-0 on error
 * @see network_mysqld_proto_get_int_len()
 */
int network_mysqld_proto_get_int8(network_packet *packet, guint8 *v) {
	guint64 v64;

	if (network_mysqld_proto_get_int_len(packet, &v64, 1)) return -1;

	g_assert_cmpint(v64 & 0xff, ==, v64); /* check that we really only got one byte back */

	*v = v64 & 0xff;

	return 0;
}

/**
 * get a 8-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param v      dest for the number
 * @return 0 on success, non-0 on error
 * @see network_mysqld_proto_get_int_len()
 */
int network_mysqld_proto_peek_int8(network_packet *packet, guint8 *v) {
	guint64 v64;

	if (network_mysqld_proto_peek_int_len(packet, &v64, 1)) return -1;

	g_assert_cmpint(v64 & 0xff, ==, v64); /* check that we really only got one byte back */

	*v = v64 & 0xff;

	return 0;
}


/**
 * get a 16-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param v      dest for the number
 * @return 0 on success, non-0 on error
 * @see network_mysqld_proto_get_int_len()
 */
int network_mysqld_proto_get_int16(network_packet *packet, guint16 *v) {
	guint64 v64;

	if (network_mysqld_proto_get_int_len(packet, &v64, 2)) return -1;

	g_assert_cmpint(v64 & 0xffff, ==, v64); /* check that we really only got two byte back */

	*v = v64 & 0xffff;

	return 0;
}

/**
 * get a 16-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param v      dest for the number
 * @return 0 on success, non-0 on error
 * @see network_mysqld_proto_get_int_len()
 */
int network_mysqld_proto_peek_int16(network_packet *packet, guint16 *v) {
	guint64 v64;

	if (network_mysqld_proto_peek_int_len(packet, &v64, 2)) return -1;

	g_assert_cmpint(v64 & 0xffff, ==, v64); /* check that we really only got two byte back */

	*v = v64 & 0xffff;

	return 0;
}


/**
 * get a 24-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param v      dest for the number
 * @return 0 on success, non-0 on error
 * @see network_mysqld_proto_get_int_len()
 */
int network_mysqld_proto_get_int24(network_packet *packet, guint32 *v) {
	guint64 v64;

	if (network_mysqld_proto_get_int_len(packet, &v64, 3)) return -1;

	g_assert_cmpint(v64 & 0x00ffffff, ==, v64); /* check that we really only got two byte back */

	*v = v64 & 0x00ffffff;

	return 0;
}


/**
 * get a 32-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param v      dest for the number
 * @return 0 on success, non-0 on error
 * @see network_mysqld_proto_get_int_len()
 */
int network_mysqld_proto_get_int32(network_packet *packet, guint32 *v) {
	guint64 v64;

	if (network_mysqld_proto_get_int_len(packet, &v64, 4)) return -1;

	*v = v64 & 0xffffffff;

	return 0;
}

/**
 * get a 6-byte integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param v      dest for the number
 * @return 0 on success, non-0 on error
 * @see network_mysqld_proto_get_int_len()
 */
int network_mysqld_proto_get_int48(network_packet *packet, guint64 *v) {
	guint64 v64;

	if (network_mysqld_proto_get_int_len(packet, &v64, 6)) return -1;

	*v = v64;

	return 0;
}

/**
 * get a 8-byte integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param v      dest for the number
 * @return 0 on success, non-0 on error
 * @see network_mysqld_proto_get_int_len()
 */
int network_mysqld_proto_get_int64(network_packet *packet, guint64 *v) {
	return network_mysqld_proto_get_int_len(packet, v, 8);
}

/**
 * find a 8-bit integer in the network packet
 *
 * @param packet the MySQL network packet
 * @param c      character to find
 * @param pos    offset into the packet the 'c' was found
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
int network_mysqld_proto_find_int8(network_packet *packet, guint8 c, guint *pos) {
	int err = 0;
	guint off = packet->offset;

	while (!err) {
		guint8 _c;

		err = err || network_mysqld_proto_get_int8(packet, &_c);
		if (!err) {
			if (c == _c) {
				*pos = packet->offset - off;
				break;
			}
		}
	}

	packet->offset = off;

	return err;
}


/**
 * get a string from the network packet
 *
 * @param packet the MySQL network packet
 * @param s      dest of the string
 * @param len    length of the string
 * @return       0 on success, non-0 otherwise
 * @return the string (allocated) or NULL of len is 0
 */
int network_mysqld_proto_get_string_len(network_packet *packet, gchar **s, gsize len) {
	gchar *str;

	if (len == 0) {
		*s = NULL;
		return 0;
	}

	if (packet->offset > packet->data->len) {
		return -1;
	}
	if (packet->offset + len > packet->data->len) {
		g_critical("%s: packet-offset out of range: %u + "F_SIZE_T" > "F_SIZE_T, 
				G_STRLOC,
				packet->offset, len, packet->data->len);

		return -1;
	}

	if (len) {
		str = g_malloc(len + 1);
		memcpy(str, packet->data->str + packet->offset, len);
		str[len] = '\0';
	} else {
		str = NULL;
	}

	packet->offset += len;

	*s = str;

	return 0;
}

/**
 * get a variable-length string from the network packet
 *
 * variable length strings are prefixed with variable-length integer defining the length of the string
 *
 * @param packet the MySQL network packet
 * @param s      destination of the decoded string
 * @param _len    destination of the length of the decoded string, if len is non-NULL
 * @return 0 on success, non-0 on error
 * @see network_mysqld_proto_get_string_len(), network_mysqld_proto_get_lenenc_int()
 */
int network_mysqld_proto_get_lenenc_string(network_packet *packet, gchar **s, guint64 *_len) {
	guint64 len;

	if (packet->offset >= packet->data->len) {
		g_debug_hexdump(G_STRLOC, S(packet->data));
		return -1;
	}	
	if (packet->offset >= packet->data->len) {
		return -1;
	}

	if (network_mysqld_proto_get_lenenc_int(packet, &len)) return -1;
	
	if (packet->offset + len > packet->data->len) return -1;

	if (_len) *_len = len;
	
	return network_mysqld_proto_get_string_len(packet, s, len);
}

/**
 * get a NUL-terminated string from the network packet
 *
 * @param packet the MySQL network packet
 * @param s      dest of the string
 * @return       0 on success, non-0 otherwise
 * @see network_mysqld_proto_get_string_len()
 */
int network_mysqld_proto_get_string(network_packet *packet, gchar **s) {
	guint64 len;
	int err = 0;

	for (len = 0; packet->offset + len < packet->data->len && *(packet->data->str + packet->offset + len); len++);

	if (*(packet->data->str + packet->offset + len) != '\0') {
		/* this has to be a \0 */
		return -1;
	}

	if (len > 0) {
		if (packet->offset >= packet->data->len) {
			return -1;
		}
		if (packet->offset + len > packet->data->len) {
			return -1;
		}

		/**
		 * copy the string w/o the NUL byte 
		 */
		err = err || network_mysqld_proto_get_string_len(packet, s, len);
	}

	err = err || network_mysqld_proto_skip(packet, 1);

	return err ? -1 : 0;
}


/**
 * get a GString from the network packet
 *
 * @param packet the MySQL network packet
 * @param len    bytes to copy
 * @param out    a GString which carries the string
 * @return       0 on success, -1 on error
 */
int network_mysqld_proto_get_gstring_len(network_packet *packet, gsize len, GString *out) {
	int err = 0;

	if (!out) return -1;

	g_string_truncate(out, 0);

	if (!len) return 0; /* nothing to copy */

	err = err || (packet->offset >= packet->data->len); /* the offset is already too large */
	err = err || (packet->offset + len > packet->data->len); /* offset would get too large */

	if (!err) {
		g_string_append_len(out, packet->data->str + packet->offset, len);
		packet->offset += len;
	}

	return err ? -1 : 0;
}

/**
 * get a NUL-terminated GString from the network packet
 *
 * @param packet the MySQL network packet
 * @param out    a GString which carries the string
 * @return       a pointer to the string in out
 *
 * @see network_mysqld_proto_get_gstring_len()
 */
int network_mysqld_proto_get_gstring(network_packet *packet, GString *out) {
	guint64 len;
	int err = 0;

	for (len = 0; packet->offset + len < packet->data->len && *(packet->data->str + packet->offset + len); len++);

	g_assert(*(packet->data->str + packet->offset + len) == '\0'); /* this has to be a \0 */

	if (len > 0) {
		g_assert(packet->offset < packet->data->len);
		g_assert(packet->offset + len <= packet->data->len);

		err = err || network_mysqld_proto_get_gstring_len(packet, len, out);
	}

	/* skip the \0 */
	err = err || network_mysqld_proto_skip(packet, 1);

	return err ? -1 : 0;
}

/**
 * get a variable-length GString from the network packet
 *
 * @param packet the MySQL network packet
 * @param out    a GString which carries the string
 * @return       0 on success, non-0 on error
 *
 * @see network_mysqld_proto_get_gstring_len(), network_mysqld_proto_get_lenenc_int()
 */
int network_mysqld_proto_get_lenenc_gstring(network_packet *packet, GString *out) {
	guint64 len;
	int err = 0;

	err = err || network_mysqld_proto_get_lenenc_int(packet, &len);
	err = err || network_mysqld_proto_get_gstring_len(packet, len, out);

	return err ? -1 : 0;
}

/**
 * create a empty field for a result-set definition
 *
 * @return a empty MYSQL_FIELD
 */
MYSQL_FIELD *network_mysqld_proto_fielddef_new() {
	MYSQL_FIELD *field;
	
	field = g_new0(MYSQL_FIELD, 1);

	return field;
}

/**
 * free a MYSQL_FIELD and its components
 *
 * @param field  the MYSQL_FIELD to free
 */
void network_mysqld_proto_fielddef_free(MYSQL_FIELD *field) {
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
GPtrArray *network_mysqld_proto_fielddefs_new(void) {
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
void network_mysqld_proto_fielddefs_free(GPtrArray *fields) {
	guint i;

	for (i = 0; i < fields->len; i++) {
		MYSQL_FIELD *field = fields->pdata[i];

		if (field) network_mysqld_proto_fielddef_free(field);
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
	return network_mysqld_proto_append_int_len(packet, num, 1);
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
	return network_mysqld_proto_append_int_len(packet, num, 2);
}

/**
 * encode 24-bit integer in to a network packet
 *
 * @param packet  the MySQL network packet
 * @param num     integer to encode
 *
 * @see network_mysqld_proto_append_int_len()
 */
int network_mysqld_proto_append_int24(GString *packet, guint32 num) {
	return network_mysqld_proto_append_int_len(packet, num, 3);
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
	return network_mysqld_proto_append_int_len(packet, num, 4);
}

/**
 * encode 48-bit integer in to a network packet
 *
 * @param packet  the MySQL network packet
 * @param num     integer to encode
 *
 * @see network_mysqld_proto_append_int_len()
 */
int network_mysqld_proto_append_int48(GString *packet, guint64 num) {
	return network_mysqld_proto_append_int_len(packet, num, 6);
}


/**
 * encode 64-bit integer in to a network packet
 *
 * @param packet  the MySQL network packet
 * @param num     integer to encode
 *
 * @see network_mysqld_proto_append_int_len()
 */
int network_mysqld_proto_append_int64(GString *packet, guint64 num) {
	return network_mysqld_proto_append_int_len(packet, num, 8);
}


/**
 * generate the response to the server challenge
 *
 * SHA1( scramble +
 *       SHA1(SHA1( password )) 
 *
 */
int network_mysqld_proto_scramble(GString *response, GString *challenge, const char *password) {
	int i;

	/* we only have SHA1() in glib 2.16.0 and higher */
#if GLIB_CHECK_VERSION(2, 16, 0)
	GChecksum *cs;
	GString *step1, *step2;

	/* first round: SHA1(password) */
	cs = g_checksum_new(G_CHECKSUM_SHA1);

	g_checksum_update(cs, (guchar *)password, strlen(password));

	step1 = g_string_sized_new(g_checksum_type_get_length(G_CHECKSUM_SHA1));

	step1->len = step1->allocated_len;
	g_checksum_get_digest(cs, (guchar *)step1->str, &(step1->len));

	g_checksum_free(cs);

	/* second round SHA1(SHA1(password)) */
	cs = g_checksum_new(G_CHECKSUM_SHA1);
	
	step2 = g_string_sized_new(g_checksum_type_get_length(G_CHECKSUM_SHA1));
	g_checksum_update(cs, (guchar *)step1->str, step1->len);
	
	step2->len = step2->allocated_len;
	g_checksum_get_digest(cs, (guchar *)step2->str, &(step2->len));

	g_checksum_free(cs);
	
	/* final round SHA1(challenge + SHA1(SHA1(password))) */
	cs = g_checksum_new(G_CHECKSUM_SHA1);
	g_checksum_update(cs, (guchar *)challenge->str, challenge->len);
	g_checksum_update(cs, (guchar *)step2->str, step2->len);
	
	g_string_set_size(response, g_checksum_type_get_length(G_CHECKSUM_SHA1));
	response->len = response->allocated_len;
	g_checksum_get_digest(cs, (guchar *)response->str, &(response->len));
	
	g_checksum_free(cs);

	/* XOR the SHA1(password) with SHA1(challenge + SHA1(SHA1(password))) */
	for (i = 0; i < 20; i++) {
		response->str[i] = (guchar)response->str[i] ^ (guchar)step1->str[i];
	}

	g_string_free(step1, TRUE);
	g_string_free(step2, TRUE);
#else
	/* we don't know how to encrypt, so fake it */
	g_string_set_size(response, 21);
	for (i = 0; i < 20; i++) response->str[i] = '\0';
	response->str[20] = '\0';
#endif
	return 0;
}

network_packet *network_packet_new(void) {
	network_packet *packet;

	packet = g_new0(network_packet, 1);

	return packet;
}

void network_packet_free(network_packet *packet) {
	if (!packet) return;

	g_free(packet);
}

int network_mysqld_proto_skip_network_header(network_packet *packet) {
	return network_mysqld_proto_skip(packet, NET_HEADER_SIZE);
}

/*@}*/
