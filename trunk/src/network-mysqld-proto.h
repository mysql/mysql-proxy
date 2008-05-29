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


/**
 * stolen from sql/log_event.h
 *
 * (MySQL 5.1.24)
 */
#define ST_SERVER_VER_LEN 50

enum Log_event_type
{
  /*
    Every time you update this enum (when you add a type), you have to
    fix Format_description_log_event::Format_description_log_event().
  */
  UNKNOWN_EVENT= 0,
  START_EVENT_V3= 1,
  QUERY_EVENT= 2,
  STOP_EVENT= 3,
  ROTATE_EVENT= 4,
  INTVAR_EVENT= 5,
  LOAD_EVENT= 6,
  SLAVE_EVENT= 7,
  CREATE_FILE_EVENT= 8,
  APPEND_BLOCK_EVENT= 9,
  EXEC_LOAD_EVENT= 10,
  DELETE_FILE_EVENT= 11,
  /*
    NEW_LOAD_EVENT is like LOAD_EVENT except that it has a longer
    sql_ex, allowing multibyte TERMINATED BY etc; both types share the
    same class (Load_log_event)
  */
  NEW_LOAD_EVENT= 12,
  RAND_EVENT= 13,
  USER_VAR_EVENT= 14,
  FORMAT_DESCRIPTION_EVENT= 15,
  XID_EVENT= 16,
  BEGIN_LOAD_QUERY_EVENT= 17,
  EXECUTE_LOAD_QUERY_EVENT= 18,
  TABLE_MAP_EVENT = 19,

  /*
    These event numbers were used for 5.1.0 to 5.1.15 and are
    therefore obsolete.
   */
  PRE_GA_WRITE_ROWS_EVENT = 20,
  PRE_GA_UPDATE_ROWS_EVENT = 21,
  PRE_GA_DELETE_ROWS_EVENT = 22,

  /*
    These event numbers are used from 5.1.16 and forward
   */
  WRITE_ROWS_EVENT = 23,
  UPDATE_ROWS_EVENT = 24,
  DELETE_ROWS_EVENT = 25,

  /*
    Something out of the ordinary happened on the master
   */
  INCIDENT_EVENT= 26,


  /*
    Add new events here - right above this comment!
    Existing events (except ENUM_END_EVENT) should never change their numbers
  */

  ENUM_END_EVENT /* end marker */
};


NETWORK_API void network_mysqld_proto_skip(GString *packet, guint *_off, gsize size);

NETWORK_API guint64 network_mysqld_proto_get_int_len(GString *packet, guint *_off, gsize size);

NETWORK_API guint8 network_mysqld_proto_get_int8(GString *packet, guint *_off);
NETWORK_API guint16 network_mysqld_proto_get_int16(GString *packet, guint *_off);
NETWORK_API guint32 network_mysqld_proto_get_int24(GString *packet, guint *_off);
NETWORK_API guint32 network_mysqld_proto_get_int32(GString *packet, guint *_off);
NETWORK_API guint64 network_mysqld_proto_get_int48(GString *packet, guint *_off);
NETWORK_API guint64 network_mysqld_proto_get_int64(GString *packet, guint *_off);

NETWORK_API int network_mysqld_proto_append_int8(GString *packet, guint8 num);
NETWORK_API int network_mysqld_proto_append_int16(GString *packet, guint16 num);
NETWORK_API int network_mysqld_proto_append_int32(GString *packet, guint32 num);


NETWORK_API gchar *network_mysqld_proto_get_lenenc_string(GString *packet, guint *_off, guint64 *_len);
NETWORK_API gchar *network_mysqld_proto_get_string_len(GString *packet, guint *_off, gsize len);
NETWORK_API gchar *network_mysqld_proto_get_string(GString *packet, guint *_off);

NETWORK_API gchar *network_mysqld_proto_get_lenenc_gstring(GString *packet, guint *_off, GString *out);
NETWORK_API gchar *network_mysqld_proto_get_gstring_len(GString *packet, guint *_off, gsize len, GString *out);
NETWORK_API gchar *network_mysqld_proto_get_gstring(GString *packet, guint *_off, GString *out);

NETWORK_API guint64 network_mysqld_proto_get_lenenc_int(GString *packet, guint *_off);

NETWORK_API int network_mysqld_proto_get_ok_packet(GString *packet, guint64 *affected, guint64 *insert_id, int *server_status, int *warning_count, char **msg);
NETWORK_API int network_mysqld_proto_append_ok_packet(GString *packet, guint64 affected_rows, guint64 insert_id, guint16 server_status, guint16 warnings);
NETWORK_API int network_mysqld_proto_append_error_packet(GString *packet, const char *errmsg, gsize errmsg_len, guint errorcode, const gchar *sqlstate);

NETWORK_API MYSQL_FIELD *network_mysqld_proto_fielddef_new(void);
NETWORK_API void network_mysqld_proto_fielddef_free(MYSQL_FIELD *fielddef);

NETWORK_API GPtrArray *network_mysqld_proto_fielddefs_new(void);
NETWORK_API void network_mysqld_proto_fielddefs_free(GPtrArray *fielddefs);

NETWORK_API size_t network_mysqld_proto_get_header(unsigned char *header);
NETWORK_API int network_mysqld_proto_set_header(unsigned char *header, size_t len, unsigned char id);

NETWORK_API int network_mysqld_proto_append_lenenc_int(GString *packet, guint64 len);
NETWORK_API int network_mysqld_proto_append_lenenc_string_len(GString *packet, const char *s, guint64 len);
NETWORK_API int network_mysqld_proto_append_lenenc_string(GString *packet, const char *s);

typedef struct {
	guint8    protocol_version;
	GString *server_version_str;
	guint32   server_version;
	guint32   thread_id;
	GString *challenge;
	guint16   capabilities;
	guint8    charset;
	guint16   status;
} network_mysqld_auth_challenge;

NETWORK_API network_mysqld_auth_challenge *network_mysqld_auth_challenge_new(void);
NETWORK_API void network_mysqld_auth_challenge_free(network_mysqld_auth_challenge *shake);
NETWORK_API int network_mysqld_proto_get_auth_challenge(GString *packet, network_mysqld_auth_challenge *shake);

typedef struct {
	guint32  capabilities;
	guint32  max_packet_size;
	guint8   charset;
	GString *username;
	GString *response;
	GString *database;
} network_mysqld_auth_response;

NETWORK_API network_mysqld_auth_response *network_mysqld_auth_response_new(void);
NETWORK_API void network_mysqld_auth_response_free(network_mysqld_auth_response *auth);
NETWORK_API int network_mysqld_proto_scramble(GString *response, GString *challenge, const char *password);
NETWORK_API int network_mysqld_proto_append_auth_response(GString *packet, network_mysqld_auth_response *auth);

/**
 * replication
 */

typedef struct {
	guint64 table_id;

	GString *db_name;
	GString *table_name;

	GPtrArray *fields;
} network_mysqld_table;

NETWORK_API network_mysqld_table *network_mysqld_table_new();
NETWORK_API void network_mysqld_table_free(network_mysqld_table *tbl);
NETWORK_API guint64 *guint64_new(guint64 i);

typedef struct {
	gchar *filename;

	/* we have to store some information from the format description event 
	 */
	guint header_len;

	/* ... and the table-ids */
	GHashTable *rbr_tables; /* hashed by table-id -> network_mysqld_table */
} network_mysqld_binlog;

NETWORK_API network_mysqld_binlog *network_mysqld_binlog_new();
NETWORK_API void network_mysqld_binlog_free(network_mysqld_binlog *binlog);

typedef struct {
	GString *data;

	guint offset;
} network_packet;

NETWORK_API network_packet *network_packet_new(void);
NETWORK_API void network_packet_free(network_packet *packet);

typedef struct {
	guint32 timestamp;
	enum Log_event_type event_type;
	guint32 server_id;
	guint32 event_size;
	guint32 log_pos;
	guint16 flags;

	union {
		struct {
			guint32 thread_id;
			guint32 exec_time;
			guint8  db_name_len;
			guint16 error_code;

			gchar *db_name;
			gchar *query;
		} query_event;
		struct {
			gchar *binlog_file;
			guint32 binlog_pos;
		} rotate_event;
		struct {
			guint16 binlog_version;
			gchar *master_version;
			guint32 created_ts;
			guint8  log_header_len;
			gchar *perm_events;
			gsize  perm_events_len;
		} format_event;
		struct {
			guint32 name_len;
			gchar *name;

			guint8 is_null;
			guint8 type;
			guint32 charset; /* charset of the string */

			guint32 value_len; 
			gchar *value; /* encoded in binary speak, depends on .type */
		} user_var_event;
		struct {
			guint64 table_id;
			guint16 flags;

			guint32 db_name_len;
			gchar *db_name;
			guint32 table_name_len;
			gchar *table_name;

			guint32 columns_len;
			gchar *columns;

			guint32 metadata_len;
			gchar *metadata;

			guint32 null_bits_len;
			gchar *null_bits;
		} table_map_event;

		struct {
			guint64 table_id;
			guint16 flags;
			
			guint32 columns_len;

			guint32 used_columns_len;
			gchar *used_columns;

			guint32 null_bits_len;
			
			guint32 row_len;
			gchar *row;      /* raw row-buffer in the format:
					    [null-bits] [field_0, ...]
					    [null-bits] [field_0, ...]
					    */
		} row_event;

		struct {
			guint8  type;
			guint64 value;
		} intvar;

		struct {
			guint64 xid_id;
		} xid;
	} event;
} network_mysqld_binlog_event;

NETWORK_API network_mysqld_binlog_event *network_mysqld_binlog_event_new(void);
NETWORK_API void network_mysqld_binlog_event_free(network_mysqld_binlog_event *event);
NETWORK_API int network_mysqld_proto_get_binlog_event_header(network_packet *packet, network_mysqld_binlog_event *event);
NETWORK_API int network_mysqld_proto_get_binlog_event(network_packet *packet, 
		network_mysqld_binlog *binlog,
		network_mysqld_binlog_event *event);
NETWORK_API int network_mysqld_proto_get_binlog_status(network_packet *packet);
NETWORK_API int network_mysqld_proto_skip_network_header(network_packet *packet);

typedef struct {
	gchar *binlog_file;
	guint32 binlog_pos;
	guint16 flags;
	guint32 server_id;
} network_mysqld_binlog_dump;

NETWORK_API network_mysqld_binlog_dump *network_mysqld_binlog_dump_new();
NETWORK_API void network_mysqld_binlog_dump_free(network_mysqld_binlog_dump *dump);
NETWORK_API int network_mysqld_proto_append_binlog_dump(GString *packet, network_mysqld_binlog_dump *dump);

#endif
