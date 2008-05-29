/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <string.h>
#include <stdio.h>
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

	g_assert_cmpint(off, <, packet->len);
	
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
	g_assert_cmpint(*_off + size, <=, packet->len);
	
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
 * get a 24-bit integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint32 network_mysqld_proto_get_int24(GString *packet, guint *_off) {
	return network_mysqld_proto_get_int_len(packet, _off, 3);
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
 * get a 6-byte integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint64 network_mysqld_proto_get_int48(GString *packet, guint *_off) {
	return network_mysqld_proto_get_int_len(packet, _off, 6);
}

/**
 * get a 8-byte integer from the network packet
 *
 * @param packet the MySQL network packet
 * @param _off   offset into the packet
 * @return a the decoded integer
 * @see network_mysqld_proto_get_int_len()
 */
guint64 network_mysqld_proto_get_int64(GString *packet, guint *_off) {
	return network_mysqld_proto_get_int_len(packet, _off, 8);
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

	if (len == 0) return NULL;

	g_assert_cmpint(*_off, <, packet->len);
	if (*_off + len > packet->len) {
		g_error("%s: packet-offset out of range: %u + "F_SIZE_T" > "F_SIZE_T, 
				G_STRLOC,
				*_off, len, packet->len);
	}

	if (len) {
		str = g_malloc(len + 1);
		memcpy(str, packet->str + *_off, len);
		str[len] = '\0';
	} else {
		str = NULL;
	}

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
gchar *network_mysqld_proto_get_lenenc_string(GString *packet, guint *_off, guint64 *_len) {
	guint64 len;

	len = network_mysqld_proto_get_lenenc_int(packet, _off);
	
	g_assert_cmpint(*_off, <, packet->len);
	g_assert_cmpint(*_off + len, <=, packet->len);

	if (_len) *_len = len;
	
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

	if (shake->server_version_str) g_string_free(shake->server_version_str, TRUE);
	if (shake->challenge)          g_string_free(shake->challenge, TRUE);

	g_free(shake);
}

int network_mysqld_proto_get_auth_challenge(GString *packet, network_mysqld_auth_challenge *shake) {
	guint off = 0;
	int maj, min, patch;
	gchar *scramble_1, *scramble_2;

	if (packet->str[NET_HEADER_SIZE + 0] == '\xff') {
		return -1;
	} else if (packet->str[NET_HEADER_SIZE + 0] != '\x0a') {
		g_critical("%s: unknown protocol", G_STRLOC);

		return -1;
	}

	/* scan for a \0 */
	for (off = NET_HEADER_SIZE + 1; packet->str[off] && off < packet->len + NET_HEADER_SIZE; off++);

	if (packet->str[off] != '\0') {
		/* the server has sent us garbage */

		g_critical("%s: protocol 10, but version number not terminated", G_STRLOC);

		return -1;
	}

	if (3 != sscanf(packet->str + NET_HEADER_SIZE + 1, "%d.%d.%d%*s", &maj, &min, &patch)) {
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

	/* skip the \0 */
	off++;

	shake->thread_id = network_mysqld_proto_get_int32(packet, &off);

	/**
	 * get the scramble buf
	 *
	 * 8 byte here and some the other 12 somewhen later
	 */	
	scramble_1 = network_mysqld_proto_get_string_len(packet, &off, 8);

	network_mysqld_proto_skip(packet, &off, 1);

	shake->capabilities  = network_mysqld_proto_get_int16(packet, &off);
	shake->charset       = network_mysqld_proto_get_int8(packet, &off);
	shake->status        = network_mysqld_proto_get_int16(packet, &off);
	
	network_mysqld_proto_skip(packet, &off, 13);
	
	scramble_2 = network_mysqld_proto_get_string_len(packet, &off, 12);
	network_mysqld_proto_skip(packet, &off, 1);

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

network_mysqld_auth_response *network_mysqld_auth_response_new() {
	network_mysqld_auth_response *auth;

	auth = g_new0(network_mysqld_auth_response, 1);

	/* we have to make sure scramble->buf is not-NULL to get
	 * the "empty string" and not a "NULL-string"
	 */
	auth->response = g_string_new("");

	auth->username = g_string_new("");

	return auth;
}

void network_mysqld_auth_response_free(network_mysqld_auth_response *auth) {
	if (!auth) return;

	if (auth->response)          g_string_free(auth->response, TRUE);
	if (auth->username)          g_string_free(auth->username, TRUE);
	if (auth->database)          g_string_free(auth->database, TRUE);

	g_free(auth);
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

/**
 * replication 
 */

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

network_packet *network_packet_new(void) {
	network_packet *packet;

	packet = g_new0(network_packet, 1);

	return packet;
}

void network_packet_free(network_packet *packet) {
	if (!packet) return;

	g_free(packet);
}

network_mysqld_binlog_event *network_mysqld_binlog_event_new() {
	network_mysqld_binlog_event *binlog;

	binlog = g_new0(network_mysqld_binlog_event, 1);

	return binlog;
}

int network_mysqld_proto_skip_network_header(network_packet *packet) {
	network_mysqld_proto_skip(packet->data, &(packet->offset), NET_HEADER_SIZE);

	return 0;
}

int network_mysqld_proto_get_binlog_status(network_packet *packet) {
	gint8 ok;

	/* on the network we have a length and packet-number of 4 bytes */
	ok = network_mysqld_proto_get_int8(packet->data, &(packet->offset));
	g_return_val_if_fail(ok == 0, -1);

	return 0;
}

int network_mysqld_proto_get_binlog_event_header(network_packet *packet, network_mysqld_binlog_event *event) {
	event->timestamp  = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
	event->event_type = network_mysqld_proto_get_int8(packet->data, &(packet->offset));
	event->server_id  = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
	event->event_size = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
	event->log_pos    = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
	event->flags      = network_mysqld_proto_get_int16(packet->data, &(packet->offset));

	return 0;
}

int network_mysqld_proto_get_binlog_event(network_packet *packet, 
		network_mysqld_binlog *binlog,
		network_mysqld_binlog_event *event) {

	network_mysqld_table *tbl;

	switch ((guchar)event->event_type) {
	case QUERY_EVENT:
		event->event.query_event.thread_id   = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
		event->event.query_event.exec_time   = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
		event->event.query_event.db_name_len = network_mysqld_proto_get_int8(packet->data, &(packet->offset));
		event->event.query_event.error_code  = network_mysqld_proto_get_int16(packet->data, &(packet->offset));

		/* 5.0 has more flags */
		if (packet->data->len > packet->offset) {
			guint16 var_size = 0;

			var_size    = network_mysqld_proto_get_int16(packet->data, &(packet->offset));
			if (var_size) {
				/* skip the variable size part for now */
				network_mysqld_proto_skip(packet->data, &(packet->offset), var_size);
			}
	
			/* default db has <db_name_len> chars */
			event->event.query_event.db_name = network_mysqld_proto_get_string_len(packet->data, &(packet->offset), 
					event->event.query_event.db_name_len);
			network_mysqld_proto_skip(packet->data, &(packet->offset), 1); /* the \0 */
	
			event->event.query_event.query = network_mysqld_proto_get_string_len(packet->data, &(packet->offset), 
					packet->data->len - packet->offset);
		}

		break;
	case ROTATE_EVENT:
		event->event.rotate_event.binlog_pos = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
		network_mysqld_proto_skip(packet->data, &(packet->offset), 4);
		event->event.rotate_event.binlog_file = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset), 
				packet->data->len - packet->offset);
		break;
	case STOP_EVENT:
		/* is empty */
		break;
	case FORMAT_DESCRIPTION_EVENT:
		event->event.format_event.binlog_version = network_mysqld_proto_get_int16(packet->data, &(packet->offset));
		event->event.format_event.master_version = network_mysqld_proto_get_string_len( /* NUL-term string */
				packet->data, &(packet->offset), ST_SERVER_VER_LEN);
		event->event.format_event.created_ts = network_mysqld_proto_get_int32(packet->data, &(packet->offset));

		/* the header length may change in the future, for now we assume it is 19 */
		event->event.format_event.log_header_len = network_mysqld_proto_get_int8(packet->data, &(packet->offset));
		g_assert_cmpint(event->event.format_event.log_header_len, ==, 19);

		/* there is some funky event-permutation going on */
		event->event.format_event.perm_events_len = packet->data->len - packet->offset;
		event->event.format_event.perm_events = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset), packet->data->len - packet->offset);
		
		break;
	case USER_VAR_EVENT:
		event->event.user_var_event.name_len = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
		event->event.user_var_event.name = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset),
				event->event.user_var_event.name_len);

		event->event.user_var_event.is_null = network_mysqld_proto_get_int8(packet->data, &(packet->offset));
		event->event.user_var_event.type = network_mysqld_proto_get_int8(packet->data, &(packet->offset));
		event->event.user_var_event.charset = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
		event->event.user_var_event.value_len = network_mysqld_proto_get_int32(packet->data, &(packet->offset));
		event->event.user_var_event.value = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset),
				event->event.user_var_event.value_len);
		break;
	case TABLE_MAP_EVENT: /* 19 */
		/**
		 * looks like a abstract definition of a table 
		 *
		 * no, we don't want to know
		 */
		event->event.table_map_event.table_id = network_mysqld_proto_get_int48(packet->data, &(packet->offset)); /* 6 bytes */
		event->event.table_map_event.flags = network_mysqld_proto_get_int16(packet->data, &(packet->offset));

		event->event.table_map_event.db_name_len = network_mysqld_proto_get_int8(packet->data, &(packet->offset));
		event->event.table_map_event.db_name = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset),
				event->event.table_map_event.db_name_len);
		network_mysqld_proto_skip(packet->data, &(packet->offset), 1); /* this should be NUL */

		event->event.table_map_event.table_name_len = network_mysqld_proto_get_int8(packet->data, &(packet->offset));
		event->event.table_map_event.table_name = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset),
				event->event.table_map_event.table_name_len);
		network_mysqld_proto_skip(packet->data, &(packet->offset), 1); /* this should be NUL */

		event->event.table_map_event.columns_len = network_mysqld_proto_get_lenenc_int(packet->data, &(packet->offset));
		event->event.table_map_event.columns = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset),
				event->event.table_map_event.columns_len);

		event->event.table_map_event.metadata_len = network_mysqld_proto_get_lenenc_int(packet->data, &(packet->offset));
		event->event.table_map_event.metadata = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset),
				event->event.table_map_event.metadata_len);

		/**
		 * the null-bit count is columns/8 
		 */

		event->event.table_map_event.null_bits_len = (int)((event->event.table_map_event.columns_len+7)/8);
		event->event.table_map_event.null_bits = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset),
				event->event.table_map_event.null_bits_len);

		g_assert_cmpint(packet->data->len, ==, packet->offset); /* this should be the full packet */
		break;
	case DELETE_ROWS_EVENT: /* 25 */
	case UPDATE_ROWS_EVENT: /* 24 */
	case WRITE_ROWS_EVENT:  /* 23 */
		event->event.row_event.table_id = network_mysqld_proto_get_int48(packet->data, &(packet->offset)); /* 6 bytes */
		event->event.row_event.flags = network_mysqld_proto_get_int16(packet->data, &(packet->offset));
		
		event->event.row_event.columns_len = network_mysqld_proto_get_lenenc_int(packet->data, &(packet->offset));

		/* a bit-mask of used-fields (m_cols.bitmap) */
		event->event.row_event.used_columns_len = (int)((event->event.row_event.columns_len+7)/8);
		event->event.row_event.used_columns = network_mysqld_proto_get_string_len(
				packet->data, &(packet->offset),
				event->event.row_event.used_columns_len);

		if (event->event_type == UPDATE_ROWS_EVENT) {
			/* the before image */
			network_mysqld_proto_skip(packet->data, &(packet->offset), event->event.row_event.used_columns_len);
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
				packet->data, &(packet->offset),
				event->event.row_event.row_len);
		
		break;
	case INTVAR_EVENT:
		event->event.intvar.type = network_mysqld_proto_get_int8(packet->data, &(packet->offset));
		event->event.intvar.value = network_mysqld_proto_get_int64(packet->data, &(packet->offset));

		break;
	case XID_EVENT:
		event->event.xid.xid_id = network_mysqld_proto_get_int64(packet->data, &(packet->offset));
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




/*@}*/
