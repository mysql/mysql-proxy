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

#ifndef _NETWORK_MYSQLD_PROTO_H_
#define _NETWORK_MYSQLD_PROTO_H_

#include <glib.h>
#ifdef _WIN32
/* mysql.h needs SOCKET defined */
#include <winsock2.h>
#endif
#include <mysql.h>

#define MYSQLD_PACKET_OK   (0)
#define MYSQLD_PACKET_RAW  (-6) /* used for proxy.response.type only */
#define MYSQLD_PACKET_NULL (-5) /* 0xfb */
                                /* 0xfc */
                                /* 0xfd */
#define MYSQLD_PACKET_EOF  (-2) /* 0xfe */
#define MYSQLD_PACKET_ERR  (-1) /* 0xff */

#define PACKET_LEN_UNSET   (0xffffffff)
#define PACKET_LEN_MAX     (0x00ffffff)

void network_mysqld_proto_skip(GString *packet, guint *_off, gsize size);

guint64 network_mysqld_proto_get_int_len(GString *packet, guint *_off, gsize size);
guint8 network_mysqld_proto_get_int8(GString *packet, guint *_off);
guint16 network_mysqld_proto_get_int16(GString *packet, guint *_off);
guint32 network_mysqld_proto_get_int32(GString *packet, guint *_off);

gchar *network_mysqld_proto_get_lenenc_string(GString *packet, guint *_off);
gchar *network_mysqld_proto_get_string_len(GString *packet, guint *_off, gsize len);
gchar *network_mysqld_proto_get_string(GString *packet, guint *_off);

gchar *network_mysqld_proto_get_lenenc_gstring(GString *packet, guint *_off, GString *out);
gchar *network_mysqld_proto_get_gstring_len(GString *packet, guint *_off, gsize len, GString *out);
gchar *network_mysqld_proto_get_gstring(GString *packet, guint *_off, GString *out);

guint64 network_mysqld_proto_decode_lenenc(GString *s, guint *_off);
int network_mysqld_proto_decode_ok_packet(GString *s, guint64 *affected, guint64 *insert_id, int *server_status, int *warning_count, char **msg);

MYSQL_FIELD *network_mysqld_proto_field_init(void);
void network_mysqld_proto_field_free(MYSQL_FIELD *field);

GPtrArray *network_mysqld_proto_fields_init(void);
void network_mysqld_proto_fields_free(GPtrArray *fields);

size_t network_mysqld_proto_get_header(unsigned char *header);
int network_mysqld_proto_set_header(unsigned char *header, size_t len, unsigned char id);

#endif
