/* $%BEGINLICENSE%$
 Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; version 2 of the
 License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA

 $%ENDLICENSE%$ */
#ifndef __NETWORK_MYSQLD_PACKET__
#define __NETWORK_MYSQLD_PACKET__

#include <glib.h>

#include "network-exports.h"

#include "network-mysqld-proto.h"
#include "network-mysqld.h"

/**
 * mid-level protocol 
 *
 * the MySQL protocal is split up in three layers:
 *
 * - low-level (encoding of fields in a packet)
 * - mid-level (encoding of packets)
 * - high-level (grouping packets into a sequence)
 */

typedef enum {
	NETWORK_MYSQLD_PROTOCOL_VERSION_PRE41,
	NETWORK_MYSQLD_PROTOCOL_VERSION_41
} network_mysqld_protocol_t;

/**
 * tracking the state of the response of a COM_QUERY packet
 */
typedef struct {
	enum {
		PARSE_COM_QUERY_INIT,
		PARSE_COM_QUERY_FIELD,
		PARSE_COM_QUERY_RESULT,
		PARSE_COM_QUERY_LOCAL_INFILE_DATA,
		PARSE_COM_QUERY_LOCAL_INFILE_RESULT
	} state;

	guint16 server_status;
	guint16 warning_count;
	guint64 affected_rows;
	guint64 insert_id;

	gboolean was_resultset;
	gboolean binary_encoded;

	guint64 rows;
	guint64 bytes;

	guint8  query_status;
} network_mysqld_com_query_result_t;

NETWORK_API network_mysqld_com_query_result_t *network_mysqld_com_query_result_new(void);
NETWORK_API void network_mysqld_com_query_result_free(network_mysqld_com_query_result_t *udata);
NETWORK_API int network_mysqld_com_query_result_track_state(network_packet *packet, network_mysqld_com_query_result_t *udata) G_GNUC_DEPRECATED;
NETWORK_API gboolean network_mysqld_com_query_result_is_load_data(network_mysqld_com_query_result_t *udata) G_GNUC_DEPRECATED;
NETWORK_API gboolean network_mysqld_com_query_result_is_local_infile(network_mysqld_com_query_result_t *udata);
NETWORK_API int network_mysqld_proto_get_com_query_result(network_packet *packet, network_mysqld_com_query_result_t *udata, gboolean use_binary_row_data);

/**
 * tracking the response of a COM_STMT_PREPARE command
 *
 * depending on the kind of statement that was prepare we will receive 0-2 EOF packets
 */
typedef struct {
	gboolean first_packet;
	gint     want_eofs;
} network_mysqld_com_stmt_prepare_result_t;

NETWORK_API network_mysqld_com_stmt_prepare_result_t *network_mysqld_com_stmt_prepare_result_new(void);
NETWORK_API void network_mysqld_com_stmt_prepare_result_free(network_mysqld_com_stmt_prepare_result_t *udata);
NETWORK_API int network_mysqld_proto_get_com_stmt_prepare_result(network_packet *packet, network_mysqld_com_stmt_prepare_result_t *udata);

/**
 * tracking the response of a COM_INIT_DB command
 *
 * we have to track the default internally can only accept it
 * if the server side OK'ed it
 */
typedef struct {
	GString *db_name;
} network_mysqld_com_init_db_result_t;

NETWORK_API network_mysqld_com_init_db_result_t *network_mysqld_com_init_db_result_new(void);
NETWORK_API void network_mysqld_com_init_db_result_free(network_mysqld_com_init_db_result_t *com_init_db);
NETWORK_API int network_mysqld_com_init_db_result_track_state(network_packet *packet, network_mysqld_com_init_db_result_t *udata);
NETWORK_API int network_mysqld_proto_get_com_init_db_result(network_packet *packet, 
		network_mysqld_com_init_db_result_t *udata,
		network_mysqld_con *con
		);

NETWORK_API int network_mysqld_proto_get_query_result(network_packet *packet, network_mysqld_con *con);
NETWORK_API int network_mysqld_con_command_states_init(network_mysqld_con *con, network_packet *packet);

NETWORK_API GList *network_mysqld_proto_get_fielddefs(GList *chunk, GPtrArray *fields);

typedef struct {
	guint64 affected_rows;
	guint64 insert_id;
	guint16 server_status;
	guint16 warnings;

	gchar *msg;
} network_mysqld_ok_packet_t;

NETWORK_API network_mysqld_ok_packet_t *network_mysqld_ok_packet_new(void);
NETWORK_API void network_mysqld_ok_packet_free(network_mysqld_ok_packet_t *udata);

NETWORK_API int network_mysqld_proto_get_ok_packet(network_packet *packet, network_mysqld_ok_packet_t *ok_packet);
NETWORK_API int network_mysqld_proto_append_ok_packet(GString *packet, network_mysqld_ok_packet_t *ok_packet);

typedef struct {
	GString *errmsg;
	GString *sqlstate;

	guint16 errcode;
	network_mysqld_protocol_t version;
} network_mysqld_err_packet_t;

NETWORK_API network_mysqld_err_packet_t *network_mysqld_err_packet_new(void);
NETWORK_API network_mysqld_err_packet_t *network_mysqld_err_packet_new_pre41(void);
NETWORK_API void network_mysqld_err_packet_free(network_mysqld_err_packet_t *udata);

NETWORK_API int network_mysqld_proto_get_err_packet(network_packet *packet, network_mysqld_err_packet_t *err_packet);
NETWORK_API int network_mysqld_proto_append_err_packet(GString *packet, network_mysqld_err_packet_t *err_packet);

typedef struct {
	guint16 server_status;
	guint16 warnings;
} network_mysqld_eof_packet_t;

NETWORK_API network_mysqld_eof_packet_t *network_mysqld_eof_packet_new(void);
NETWORK_API void network_mysqld_eof_packet_free(network_mysqld_eof_packet_t *udata);

NETWORK_API int network_mysqld_proto_get_eof_packet(network_packet *packet, network_mysqld_eof_packet_t *eof_packet);
NETWORK_API int network_mysqld_proto_append_eof_packet(GString *packet, network_mysqld_eof_packet_t *eof_packet);

struct network_mysqld_auth_challenge {
	guint8    protocol_version;
	gchar    *server_version_str;
	guint32   server_version;
	guint32   thread_id;
	GString  *challenge;
	guint16   capabilities;
	guint8    charset;
	guint16   server_status;
};

NETWORK_API network_mysqld_auth_challenge *network_mysqld_auth_challenge_new(void);
NETWORK_API void network_mysqld_auth_challenge_free(network_mysqld_auth_challenge *shake);
NETWORK_API int network_mysqld_proto_get_auth_challenge(network_packet *packet, network_mysqld_auth_challenge *shake);
NETWORK_API int network_mysqld_proto_append_auth_challenge(GString *packet, network_mysqld_auth_challenge *shake);
NETWORK_API void network_mysqld_auth_challenge_set_challenge(network_mysqld_auth_challenge *shake);

struct network_mysqld_auth_response {
	guint32  capabilities;
	guint32  max_packet_size;
	guint8   charset;
	GString *username;
	GString *response;
	GString *database;
};

NETWORK_API network_mysqld_auth_response *network_mysqld_auth_response_new(void);
NETWORK_API void network_mysqld_auth_response_free(network_mysqld_auth_response *auth);
NETWORK_API int network_mysqld_proto_append_auth_response(GString *packet, network_mysqld_auth_response *auth);
NETWORK_API int network_mysqld_proto_get_auth_response(network_packet *packet, network_mysqld_auth_response *auth);
NETWORK_API network_mysqld_auth_response *network_mysqld_auth_response_copy(network_mysqld_auth_response *src);

/* COM_STMT_* */

/*
 * COM_STMT_PREPARE
 *   -> \x16 string
 * 
 *  1c 00 00 00 16 53 45 4c    45 43 54 20 43 4f 4e 43    .....SELECT CONC
 *  41 54 28 3f 2c 20 3f 29    20 41 53 20 63 6f 6c 31    AT(?, ?) AS col1
 */

typedef struct {
	GString *stmt_text;
} network_mysqld_stmt_prepare_packet_t;

NETWORK_API network_mysqld_stmt_prepare_packet_t *network_mysqld_stmt_prepare_packet_new();
NETWORK_API void network_mysqld_stmt_prepare_packet_free(network_mysqld_stmt_prepare_packet_t *stmt_prepare_packet);
NETWORK_API int network_mysqld_proto_get_stmt_prepare_packet(network_packet *packet, network_mysqld_stmt_prepare_packet_t *stmt_prepare_packet);
NETWORK_API int network_mysqld_proto_append_stmt_prepare_packet(GString *packet, network_mysqld_stmt_prepare_packet_t *stmt_prepare_packet);

/**
 * COM_STMT_PREPARE OK
 *     \x00 
 *        4-byte stmt-id
 *        2-byte num-col
 *        2-byte num-params
 *        1-byte filler
 *        2-byte warning count
 *    is followed by some extra packets which are handled elsewhere:
 *        num-params * <param-defs> <EOF> if num-params > 0
 *        num-colums * <column-defs> <EOF> if num-columns > 0
 *
 *  0c 00 00 01 00 01 00 00    00 01 00 02 00 00 00 00|   ................
 *  17 00 00 02 03 64 65 66    00 00 00 01 3f 00 0c 3f    .....def....?..?
 *  00 00 00 00 00 fd 80 00    00 00 00|17 00 00 03 03    ................
 *  64 65 66 00 00 00 01 3f    00 0c 3f 00 00 00 00 00    def....?..?.....
 *  fd 80 00 00 00 00|05 00    00 04 fe 00 00 02 00|1a    ................
 *  00 00 05 03 64 65 66 00    00 00 04 63 6f 6c 31 00    ....def....col1.
 *  0c 3f 00 00 00 00 00 fd    80 00 1f 00 00|05 00 00    .?..............
 *  06 fe 00 00 02 00                                     ...... 
 */

typedef struct {
	guint32 stmt_id;
	guint16 num_columns;
	guint16 num_params;
	guint16 warnings;
} network_mysqld_stmt_prepare_ok_packet_t;

NETWORK_API network_mysqld_stmt_prepare_ok_packet_t *network_mysqld_stmt_prepare_ok_packet_new(void);
NETWORK_API void network_mysqld_stmt_prepare_ok_packet_free(network_mysqld_stmt_prepare_ok_packet_t *stmt_prepare_ok_packet);
NETWORK_API int network_mysqld_proto_get_stmt_prepare_ok_packet(network_packet *packet, network_mysqld_stmt_prepare_ok_packet_t *stmt_prepare_ok_packet);
NETWORK_API int network_mysqld_proto_append_stmt_prepare_ok_packet(GString *packet, network_mysqld_stmt_prepare_ok_packet_t *stmt_prepare_ok_packet);

/*
 * COM_STMT_EXECUTE
 *   -> \x17
 *        4-byte stmt-id
 *        1-byte flags
 *        4-byte iteration-count
 *        nul-bit-map
 *        1-byte new-params-bound-flag
 *        n*2 type-of-param if new-params-bound
 *        <params>
 *
 *  18 00 00 00.17.01 00 00    00.00.01 00 00 00.00.01    ................
 *  fe 00.fe 00.03 66 6f 6f.   03 62 61 72                .....foo.bar
 */

typedef struct {
	guint32 stmt_id;
	guint8  flags;
	guint32 iteration_count;
	GString *nul_bits; /**< a NULL-bitmap, size is (params * 7)/8 */
	guint8 new_params_bound;
	GPtrArray *params; /**< array<network_mysqld_type *> */
} network_mysqld_stmt_execute_packet_t;

NETWORK_API network_mysqld_stmt_execute_packet_t *network_mysqld_stmt_execute_packet_new(void);
NETWORK_API void network_mysqld_stmt_execute_packet_free(network_mysqld_stmt_execute_packet_t *stmt_execute_packet);
NETWORK_API int network_mysqld_proto_get_stmt_execute_packet(network_packet *packet, network_mysqld_stmt_execute_packet_t *stmt_execute_packet);
NETWORK_API int network_mysqld_proto_append_stmt_execute_packet(GString *packet, network_mysqld_stmt_execute_packet_t *stmt_execute_packet);


/*
 * COM_STMT_EXECUTE resultset
 *   -> field-packet
 *     lenenc field-count
 *     field-count * fielddef
 *     <EOF>
 *     <rows>
 *     <EOF>
 *
 * the header is the same as for normal resultsets:
 *   @see network_mysqld_proto_get_fielddefs()
 *
 *  01 00 00 01 01|1a 00 00    02 03 64 65 66 00 00 00    ..........def...
 *  04 63 6f 6c 31 00 0c 08    00 06 00 00 00 fd 00 00    .col1...........
 *  1f 00 00|05 00 00 03 fe    00 00 02 00|09 00 00 04    ................
 *  00 00 06 66 6f 6f 62 61    72|05 00 00 05 fe 00 00    ...foobar.......
 *  02 00                                                 ..     
 */

typedef GPtrArray network_mysqld_resultset_row_t;

NETWORK_API network_mysqld_resultset_row_t *network_mysqld_resultset_row_new(void);
NETWORK_API void network_mysqld_resultset_row_free(network_mysqld_resultset_row_t *row);
NETWORK_API GList *network_mysqld_proto_get_next_binary_row(GList *chunk, network_mysqld_proto_fielddefs_t *fields, network_mysqld_resultset_row_t *row);


/*
 * COM_STMT_CLOSE
 *   -> \x19
 *        4-byte stmt-id
 *
 * 05 00 00 00 19 01 00 00    00                         ......... 
 */
typedef struct {
	guint32 stmt_id;
} network_mysqld_stmt_close_packet_t;

NETWORK_API network_mysqld_stmt_close_packet_t *network_mysqld_stmt_close_packet_new(void);
NETWORK_API void network_mysqld_stmt_close_packet_free(network_mysqld_stmt_close_packet_t *stmt_close_packet);
NETWORK_API int network_mysqld_proto_get_stmt_close_packet(network_packet *packet, network_mysqld_stmt_close_packet_t *stmt_close_packet);
NETWORK_API int network_mysqld_proto_append_stmt_close_packet(GString *packet, network_mysqld_stmt_close_packet_t *stmt_close_packet);

#endif
