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

/**
 * tracking the state of the response of a COM_QUERY packet
 */
typedef struct {
	enum {
		PARSE_COM_QUERY_INIT,
		PARSE_COM_QUERY_FIELD,
		PARSE_COM_QUERY_RESULT,
		PARSE_COM_QUERY_LOAD_DATA,
		PARSE_COM_QUERY_LOAD_DATA_END_DATA
	} state;

	guint16 server_status;
	guint16 warning_count;
	guint64 affected_rows;
	guint64 insert_id;

	gboolean was_resultset;

	guint64 rows;
	guint64 bytes;
} network_mysqld_com_query_result_t;

NETWORK_API network_mysqld_com_query_result_t *network_mysqld_com_query_result_new(void);
NETWORK_API void network_mysqld_com_query_result_free(network_mysqld_com_query_result_t *udata);
NETWORK_API int network_mysqld_com_query_result_track_state(network_packet *packet, network_mysqld_com_query_result_t *udata);
NETWORK_API gboolean network_mysqld_com_query_result_is_load_data(network_mysqld_com_query_result_t *udata);
NETWORK_API int network_mysqld_proto_get_com_query_result(network_packet *packet, network_mysqld_com_query_result_t *udata);

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
NETWORK_API GList *network_mysqld_proto_get_fielddefs(GList *chunk, GPtrArray *fields);

#endif
