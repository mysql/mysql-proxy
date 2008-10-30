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
 

#ifndef _NETWORK_MYSQLD_PROTO_H_
#define _NETWORK_MYSQLD_PROTO_H_

#include <glib.h>
#ifdef _WIN32
/* mysql.h needs SOCKET defined */
#include <winsock2.h>
#endif
#include <mysql.h>

#include "network-exports.h"
/**
 * 4.0 is missing too many things for us to support it, so we have to error out.
 */
#if MYSQL_VERSION_ID < 41000
#error You need at least MySQL 4.1 to compile this software. 
#endif
/**
 * 4.1 uses other defines
 *
 * this should be one step to get closer to backward-compatibility
 */
#if MYSQL_VERSION_ID < 50000
#define COM_STMT_EXECUTE        COM_EXECUTE
#define COM_STMT_PREPARE        COM_PREPARE
#define COM_STMT_CLOSE          COM_CLOSE_STMT
#define COM_STMT_SEND_LONG_DATA COM_LONG_DATA
#define COM_STMT_RESET          COM_RESET_STMT
#endif

#define MYSQLD_PACKET_OK   (0)
#define MYSQLD_PACKET_RAW  (0xfa) /* used for proxy.response.type only */
#define MYSQLD_PACKET_NULL (0xfb) /* 0xfb */
                                  /* 0xfc */
                                  /* 0xfd */
#define MYSQLD_PACKET_EOF  (0xfe) /* 0xfe */
#define MYSQLD_PACKET_ERR  (0xff) /* 0xff */

#define PACKET_LEN_UNSET   (0xffffffff)
#define PACKET_LEN_MAX     (0x00ffffff)

typedef struct {
	GString *data;

	guint offset;
} network_packet;

NETWORK_API network_packet *network_packet_new(void);
NETWORK_API void network_packet_free(network_packet *packet);



NETWORK_API int network_mysqld_proto_skip(network_packet *packet, gsize size);
NETWORK_API int network_mysqld_proto_skip_network_header(network_packet *packet);

NETWORK_API int network_mysqld_proto_get_int_len(network_packet *packet, guint64 *v, gsize size);

NETWORK_API int network_mysqld_proto_get_int8(network_packet *packet, guint8 *v);
NETWORK_API int network_mysqld_proto_get_int16(network_packet *packet, guint16 *v);
NETWORK_API int network_mysqld_proto_get_int24(network_packet *packet, guint32 *v);
NETWORK_API int network_mysqld_proto_get_int32(network_packet *packet, guint32 *v);
NETWORK_API int network_mysqld_proto_get_int48(network_packet *packet, guint64 *v);
NETWORK_API int network_mysqld_proto_get_int64(network_packet *packet, guint64 *v);

NETWORK_API int network_mysqld_proto_peek_int_len(network_packet *packet, guint64 *v, gsize size);
NETWORK_API int network_mysqld_proto_peek_int8(network_packet *packet, guint8 *v);
NETWORK_API int network_mysqld_proto_peek_int16(network_packet *packet, guint16 *v);
NETWORK_API int network_mysqld_proto_find_int8(network_packet *packet, guint8 c, guint *pos);

NETWORK_API int network_mysqld_proto_append_int8(GString *packet, guint8 num);
NETWORK_API int network_mysqld_proto_append_int16(GString *packet, guint16 num);
NETWORK_API int network_mysqld_proto_append_int24(GString *packet, guint32 num);
NETWORK_API int network_mysqld_proto_append_int32(GString *packet, guint32 num);
NETWORK_API int network_mysqld_proto_append_int48(GString *packet, guint64 num);
NETWORK_API int network_mysqld_proto_append_int64(GString *packet, guint64 num);


NETWORK_API int network_mysqld_proto_get_lenenc_string(network_packet *packet, gchar **s, guint64 *_len);
NETWORK_API int network_mysqld_proto_get_string_len(network_packet *packet, gchar **s, gsize len);
NETWORK_API int network_mysqld_proto_get_string(network_packet *packet, gchar **s);

NETWORK_API int network_mysqld_proto_get_lenenc_gstring(network_packet *packet, GString *out);
NETWORK_API int network_mysqld_proto_get_gstring_len(network_packet *packet, gsize len, GString *out);
NETWORK_API int network_mysqld_proto_get_gstring(network_packet *packet, GString *out);

NETWORK_API int network_mysqld_proto_get_lenenc_int(network_packet *packet, guint64 *v);

NETWORK_API MYSQL_FIELD *network_mysqld_proto_fielddef_new(void);
NETWORK_API void network_mysqld_proto_fielddef_free(MYSQL_FIELD *fielddef);

NETWORK_API GPtrArray *network_mysqld_proto_fielddefs_new(void);
NETWORK_API void network_mysqld_proto_fielddefs_free(GPtrArray *fielddefs);

NETWORK_API size_t network_mysqld_proto_get_header(unsigned char *header);
NETWORK_API int network_mysqld_proto_set_header(unsigned char *header, size_t len, unsigned char id);

NETWORK_API int network_mysqld_proto_append_lenenc_int(GString *packet, guint64 len);
NETWORK_API int network_mysqld_proto_append_lenenc_string_len(GString *packet, const char *s, guint64 len);
NETWORK_API int network_mysqld_proto_append_lenenc_string(GString *packet, const char *s);

NETWORK_API int network_mysqld_proto_scramble(GString *response, GString *challenge, const char *password);

#endif
