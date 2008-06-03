/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <string.h>
#include <stdio.h>
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
 * a handy marco for constant strings 
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
 * @param _off     offset in into the packet 
 * @return the decoded number
 *
 */
guint64 network_mysqld_proto_get_lenenc_int(network_packet *packet) {
	guint off = packet->offset;
	guint64 ret = 0;
	unsigned char *bytestream = (unsigned char *)packet->data->str;

	g_assert_cmpint(off, <, packet->data->len);
	
	if (bytestream[off] < 251) { /* */
		ret = bytestream[off];
	} else if (bytestream[off] == 251) { /* NULL in row-data */
		ret = bytestream[off];
	} else if (bytestream[off] == 252) { /* 2 byte length*/
		g_assert(off + 2 < packet->data->len);
		ret = (bytestream[off + 1] << 0) | 
			(bytestream[off + 2] << 8) ;
		off += 2;
	} else if (bytestream[off] == 253) { /* 3 byte */
		g_assert(off + 3 < packet->data->len);
		ret = (bytestream[off + 1]   <<  0) | 
			(bytestream[off + 2] <<  8) |
			(bytestream[off + 3] << 16);

		off += 3;
	} else if (bytestream[off] == 254) { /* 8 byte */
		g_assert(off + 8 < packet->data->len);
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

	packet->offset = off;

	return ret;
}

/**
 * decode a OK packet from the network packet
 */
int network_mysqld_proto_get_ok_packet(network_packet *packet, guint64 *affected, guint64 *insert_id, int *server_status, int *warning_count, char **msg) {
	guint64 dest;
	guint field_count;

	field_count = network_mysqld_proto_get_int8(packet);
	g_assert(field_count == 0);

	dest = network_mysqld_proto_get_lenenc_int(packet); if (affected) *affected = dest;
	dest = network_mysqld_proto_get_lenenc_int(packet); if (insert_id) *insert_id = dest;

	dest = network_mysqld_proto_get_int16(packet);      if (server_status) *server_status = dest;
	dest = network_mysqld_proto_get_int16(packet);      if (warning_count) *warning_count = dest;

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
void network_mysqld_proto_skip(network_packet *packet, gsize size) {
	g_assert_cmpint(packet->offset + size, <=, packet->data->len);
	
	packet->offset += size;
}

/**
 * get a fixed-length integer from the network packet 
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @param size   byte-len of the integer to decode
 * @return a the decoded integer
 */
guint64 network_mysqld_proto_get_int_len(network_packet *packet, gsize size) {
	gsize i;
	int shift;
	guint64 r = 0;

	g_assert(packet->offset < packet->data->len);
	if (packet->offset + size > packet->data->len) {
		CRASHME();
	}
	g_assert(packet->offset + size <= packet->data->len);

	for (i = 0, shift = 0; i < size; i++, shift += 8) {
		r += (unsigned char)(packet->data->str[packet->offset + i]) << shift;
	}

	packet->offset += size;

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
guint8 network_mysqld_proto_get_int8(network_packet *packet) {
	return network_mysqld_proto_get_int_len(packet, 1);
}

/**
 * get a 16-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint16 network_mysqld_proto_get_int16(network_packet *packet) {
	return network_mysqld_proto_get_int_len(packet, 2);
}

/**
 * get a 24-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint32 network_mysqld_proto_get_int24(network_packet *packet) {
	return network_mysqld_proto_get_int_len(packet, 3);
}


/**
 * get a 32-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint32 network_mysqld_proto_get_int32(network_packet *packet) {
	return network_mysqld_proto_get_int_len(packet, 4);
}

/**
 * get a 6-byte integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint64 network_mysqld_proto_get_int48(network_packet *packet) {
	return network_mysqld_proto_get_int_len(packet, 6);
}

/**
 * get a 8-byte integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint64 network_mysqld_proto_get_int64(network_packet *packet) {
	return network_mysqld_proto_get_int_len(packet, 8);
}

/**
 * get a string from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @param len    length of the string
 * @return the string (allocated) or NULL of len is 0
 */
gchar *network_mysqld_proto_get_string_len(network_packet *packet, gsize len) {
	gchar *str;

	if (len == 0) return NULL;

	g_assert_cmpint(packet->offset, <, packet->data->len);
	if (packet->offset + len > packet->data->len) {
		g_error("%s: packet-offset out of range: %u + "F_SIZE_T" > "F_SIZE_T, 
				G_STRLOC,
				packet->offset, len, packet->data->len);
	}

	if (len) {
		str = g_malloc(len + 1);
		memcpy(str, packet->data->str + packet->offset, len);
		str[len] = '\0';
	} else {
		str = NULL;
	}

	packet->offset += len;

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
gchar *network_mysqld_proto_get_lenenc_string(network_packet *packet, guint64 *_len) {
	guint64 len;

	if (packet->offset >= packet->data->len) {
		g_debug_hexdump(G_STRLOC, S(packet->data));
		CRASHME();
	}	
	g_assert_cmpint(packet->offset, <, packet->data->len);

	len = network_mysqld_proto_get_lenenc_int(packet);
	
	g_assert_cmpint(packet->offset + len, <=, packet->data->len);

	if (_len) *_len = len;
	
	return network_mysqld_proto_get_string_len(packet, len);
}

/**
 * get a NUL-terminated string from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return       the string
 * @see network_mysqld_proto_get_string_len()
 */
gchar *network_mysqld_proto_get_string(network_packet *packet) {
	guint len;
	gchar *r = NULL;

	for (len = 0; packet->offset + len < packet->data->len && *(packet->data->str + packet->offset + len); len++);

	g_assert(*(packet->data->str + packet->offset + len) == '\0'); /* this has to be a \0 */

	if (len > 0) {
		g_assert(packet->offset < packet->data->len);
		g_assert(packet->offset + len <= packet->data->len);

		/**
		 * copy the string w/o the NUL byte 
		 */
		r = network_mysqld_proto_get_string_len(packet, len);
	}

	packet->offset += 1;

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
gchar *network_mysqld_proto_get_gstring_len(network_packet *packet, gsize len, GString *out) {
	g_string_truncate(out, 0);

	if (len) {
		g_assert(packet->offset < packet->data->len);
		if (packet->offset + len > packet->data->len) {
			g_error("packet-offset out of range: %u + "F_SIZE_T" > "F_SIZE_T, packet->offset, len, packet->data->len);
		}

		g_string_append_len(out, packet->data->str + packet->offset, len);
		packet->offset += len;
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
gchar *network_mysqld_proto_get_gstring(network_packet *packet, GString *out) {
	guint len;
	gchar *r = NULL;

	for (len = 0; packet->offset + len < packet->data->len && *(packet->data->str + packet->offset + len); len++);

	g_assert(*(packet->data->str + packet->offset + len) == '\0'); /* this has to be a \0 */

	if (len > 0) {
		g_assert(packet->offset < packet->data->len);
		g_assert(packet->offset + len <= packet->data->len);

		r = network_mysqld_proto_get_gstring_len(packet, len, out);
	}

	/* skip the \0 */
	packet->offset += 1;

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
gchar *network_mysqld_proto_get_lenenc_gstring(network_packet *packet, GString *out) {
	guint64 len;

	len = network_mysqld_proto_get_lenenc_int(packet);

	return network_mysqld_proto_get_gstring_len(packet, len, out);
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

/**
 * high-level functions
 */

network_mysqld_auth_challenge *network_mysqld_auth_challenge_new() {
	network_mysqld_auth_challenge *shake;

	shake = g_new0(network_mysqld_auth_challenge, 1);
	
	shake->challenge = g_string_new("");

	return shake;
}

void network_mysqld_auth_challenge_free(network_mysqld_auth_challenge *shake) {
	if (!shake) return;

	if (shake->server_version_str) g_free(shake->server_version_str);
	if (shake->challenge)          g_string_free(shake->challenge, TRUE);

	g_free(shake);
}

int network_mysqld_proto_get_auth_challenge(network_packet *packet, network_mysqld_auth_challenge *shake) {
	int maj, min, patch;
	gchar *scramble_1, *scramble_2;
	guint8 status;

	network_mysqld_proto_skip_network_header(packet);

	status = network_mysqld_proto_get_int8(packet);
	switch (status) {
	case 0xff:
		return -1;
	case 0x0a:
		break;
	default:
		g_critical("%s: unknown protocol", G_STRLOC);
		return -1;
	}

	shake->server_version_str = network_mysqld_proto_get_string(packet);

	if (3 != sscanf(shake->server_version_str, "%d.%d.%d%*s", &maj, &min, &patch)) {
		/* can't parse the protocol */

		g_critical("%s: protocol 10, but version number not parsable", G_STRLOC);

		return -1;
	}

	/**
	 * out of range 
	 */
	if (min   < 0 || min   > 100 ||
	    patch < 0 || patch > 100 ||
	    maj   < 0 || maj   > 10) {
		g_critical("%s: protocol 10, but version number out of range", G_STRLOC);

		return -1;
	}

	shake->server_version = 
		maj * 10000 +
		min *   100 +
		patch;

	shake->thread_id = network_mysqld_proto_get_int32(packet);

	/**
	 * get the scramble buf
	 *
	 * 8 byte here and some the other 12 somewhen later
	 */	
	scramble_1 = network_mysqld_proto_get_string_len(packet, 8);

	network_mysqld_proto_skip(packet, 1);

	shake->capabilities  = network_mysqld_proto_get_int16(packet);
	shake->charset       = network_mysqld_proto_get_int8(packet);
	shake->status        = network_mysqld_proto_get_int16(packet);
	
	network_mysqld_proto_skip(packet, 13);
	
	scramble_2 = network_mysqld_proto_get_string_len(packet, 12);
	network_mysqld_proto_skip(packet, 1);

	/**
	 * scramble_1 + scramble_2 == scramble
	 *
	 * a len-encoded string
	 */

	g_string_truncate(shake->challenge, 0);
	g_string_append_len(shake->challenge, scramble_1, 8);
	g_string_append_len(shake->challenge, scramble_2, 12);

	g_free(scramble_1);
	g_free(scramble_2);

	return 0;
}

int network_mysqld_proto_append_auth_challenge(GString *packet, network_mysqld_auth_challenge *shake) {
	guint i;

	network_mysqld_proto_append_int8(packet, 0x0a);
	g_string_append(packet, shake->server_version_str);
	network_mysqld_proto_append_int8(packet, 0x00);
	network_mysqld_proto_append_int32(packet, shake->thread_id);
	g_string_append_len(packet, shake->challenge->str, 8);
	network_mysqld_proto_append_int8(packet, 0x00); /* filler */
	network_mysqld_proto_append_int16(packet, shake->capabilities);
	network_mysqld_proto_append_int8(packet, shake->charset);
	network_mysqld_proto_append_int16(packet, shake->status);

	for (i = 0; i < 13; i++) {
		network_mysqld_proto_append_int8(packet, 0x00);
	}

	g_string_append_len(packet, shake->challenge->str + 8, 12);
	network_mysqld_proto_append_int8(packet, 0x00);
	
	return 0;
}

network_mysqld_auth_response *network_mysqld_auth_response_new() {
	network_mysqld_auth_response *auth;

	auth = g_new0(network_mysqld_auth_response, 1);

	/* we have to make sure scramble->buf is not-NULL to get
	 * the "empty string" and not a "NULL-string"
	 */
	auth->response = g_string_new("");
	auth->username = g_string_new("");
	auth->database = g_string_new("");

	return auth;
}

void network_mysqld_auth_response_free(network_mysqld_auth_response *auth) {
	if (!auth) return;

	if (auth->response)          g_string_free(auth->response, TRUE);
	if (auth->username)          g_string_free(auth->username, TRUE);
	if (auth->database)          g_string_free(auth->database, TRUE);

	g_free(auth);
}

int network_mysqld_proto_get_auth_response(network_packet *packet, network_mysqld_auth_response *auth) {
	/* extract the default db from it */
	network_mysqld_proto_skip_network_header(packet); /* packet-header */

	/*
	 * @\0\0\1
	 *  \215\246\3\0 - client-flags
	 *  \0\0\0\1     - max-packet-len
	 *  \10          - charset-num
	 *  \0\0\0\0
	 *  \0\0\0\0
	 *  \0\0\0\0
	 *  \0\0\0\0
	 *  \0\0\0\0
	 *  \0\0\0       - fillers
	 *  root\0       - username
	 *  \24          - len of the scrambled buf
	 *    ~    \272 \361 \346
	 *    \211 \353 D    \351
	 *    \24  \243 \223 \257
	 *    \0   ^    \n   \254
	 *    t    \347 \365 \244
	 *  
	 *  world\0
	 */

	auth->capabilities    = network_mysqld_proto_get_int32(packet);
	auth->max_packet_size = network_mysqld_proto_get_int32(packet);
	auth->charset         = network_mysqld_proto_get_int8(packet);

	network_mysqld_proto_skip(packet, 23);
	
	network_mysqld_proto_get_gstring(packet, auth->username);
	network_mysqld_proto_get_lenenc_gstring(packet, auth->response);

	if (packet->offset != packet->data->len) {
		network_mysqld_proto_get_gstring(packet, auth->database);
	}

	return 0;
}

/**
 * append the auth struct to the mysqld packet
 */
int network_mysqld_proto_append_auth_response(GString *packet, network_mysqld_auth_response *auth) {
	int i;

	network_mysqld_proto_append_int32(packet, auth->capabilities);
	network_mysqld_proto_append_int32(packet, auth->max_packet_size); /* max-allowed-packet */
	
	network_mysqld_proto_append_int8(packet, auth->charset); /* charset */

	for (i = 0; i < 23; i++) { /* filler */
		network_mysqld_proto_append_int8(packet, 0x00);
	}

	g_string_append(packet, auth->username->str);
	network_mysqld_proto_append_int8(packet, 0x00); /* trailing \0 */

	/* scrambled password */
	network_mysqld_proto_append_lenenc_string_len(packet, auth->response->str, auth->response->len);

	return 0;
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

#else
	/* we don't know how to encrypt, so fake it */
	g_string_set_size(response, 20);
	for (i = 0; i < 20; i++) response->str[i] = '\0';
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
	network_mysqld_proto_skip(packet, NET_HEADER_SIZE);

	return 0;
}

/*@}*/
