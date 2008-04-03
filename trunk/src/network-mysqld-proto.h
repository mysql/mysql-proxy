/* Copyright (C) 2007, 2008 MySQL AB */ 

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
#define MYSQLD_PACKET_RAW  (-6) /* used for proxy.response.type only */
#define MYSQLD_PACKET_NULL (-5) /* 0xfb */
                                /* 0xfc */
                                /* 0xfd */
#define MYSQLD_PACKET_EOF  (-2) /* 0xfe */
#define MYSQLD_PACKET_ERR  (-1) /* 0xff */

#define PACKET_LEN_UNSET   (0xffffffff)
#define PACKET_LEN_MAX     (0x00ffffff)

NETWORK_API void network_mysqld_proto_skip(GString *packet, guint *_off, gsize size);

NETWORK_API guint64 network_mysqld_proto_get_int_len(GString *packet, guint *_off, gsize size);

NETWORK_API guint8 network_mysqld_proto_get_int8(GString *packet, guint *_off);
NETWORK_API guint16 network_mysqld_proto_get_int16(GString *packet, guint *_off);
NETWORK_API guint32 network_mysqld_proto_get_int32(GString *packet, guint *_off);

NETWORK_API int network_mysqld_proto_append_int8(GString *packet, guint8 num);
NETWORK_API int network_mysqld_proto_append_int16(GString *packet, guint16 num);
NETWORK_API int network_mysqld_proto_append_int32(GString *packet, guint32 num);


NETWORK_API gchar *network_mysqld_proto_get_lenenc_string(GString *packet, guint *_off);
NETWORK_API gchar *network_mysqld_proto_get_string_len(GString *packet, guint *_off, gsize len);
NETWORK_API gchar *network_mysqld_proto_get_string(GString *packet, guint *_off);

NETWORK_API gchar *network_mysqld_proto_get_lenenc_gstring(GString *packet, guint *_off, GString *out);
NETWORK_API gchar *network_mysqld_proto_get_gstring_len(GString *packet, guint *_off, gsize len, GString *out);
NETWORK_API gchar *network_mysqld_proto_get_gstring(GString *packet, guint *_off, GString *out);

NETWORK_API guint64 network_mysqld_proto_get_lenenc_int(GString *packet, guint *_off);

NETWORK_API int network_mysqld_proto_get_ok_packet(GString *packet, guint64 *affected, guint64 *insert_id, int *server_status, int *warning_count, char **msg);
NETWORK_API int network_mysqld_proto_append_ok_packet(GString *packet, guint64 affected_rows, guint64 insert_id, guint16 server_status, guint16 warnings);
NETWORK_API int network_mysqld_proto_append_error_packet(GString *packet, const char *errmsg, gsize errmsg_len, guint errorcode, const gchar *sqlstate);

NETWORK_API MYSQL_FIELD *network_mysqld_proto_field_init(void);
NETWORK_API void network_mysqld_proto_field_free(MYSQL_FIELD *field);

NETWORK_API GPtrArray *network_mysqld_proto_fields_init(void);
NETWORK_API void network_mysqld_proto_fields_free(GPtrArray *fields);

NETWORK_API size_t network_mysqld_proto_get_header(unsigned char *header);
NETWORK_API int network_mysqld_proto_set_header(unsigned char *header, size_t len, unsigned char id);

NETWORK_API int network_mysqld_proto_append_lenenc_int(GString *packet, guint64 len);
NETWORK_API int network_mysqld_proto_append_lenenc_string_len(GString *packet, const char *s, guint64 len);
NETWORK_API int network_mysqld_proto_append_lenenc_string(GString *packet, const char *s);

typedef struct {
	gint8    protocol_version;
	GString *server_version_str;
	gint32   server_version;
	gint32   thread_id;
	GString *challenge;
	gint16   capabilities;
	gint8    charset;
	gint16   status;
} network_mysqld_handshake;

NETWORK_API network_mysqld_handshake *network_mysqld_handshake_new(void);
NETWORK_API void network_mysqld_handshake_free(network_mysqld_handshake *shake);
NETWORK_API int network_mysqld_proto_get_handshake(GString *packet, network_mysqld_handshake *shake);

typedef struct {
	guint32  capabilities;
	guint32  max_packet_size;
	guint8   charset;
	GString *username;
	GString *response;
	GString *database;
} network_mysqld_auth;

NETWORK_API network_mysqld_auth *network_mysqld_auth_new(void);
NETWORK_API void network_mysqld_auth_free(network_mysqld_auth *auth);
NETWORK_API int network_mysqld_proto_scramble(GString *response, GString *challenge, const char *password);
NETWORK_API int network_mysqld_proto_append_auth(GString *packet, network_mysqld_auth *auth);


#endif
