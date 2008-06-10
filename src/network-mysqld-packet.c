/**
 * codec's for the MySQL client protocol
 */

#include "network-mysqld-packet.h"
#define C(x) x, sizeof(x) - 1
#define S(x) x->str, x->len

network_mysqld_com_query_result_t *network_mysqld_com_query_result_new() {
	network_mysqld_com_query_result_t *com_query;

	com_query = g_new0(network_mysqld_com_query_result_t, 1);
	com_query->state = PARSE_COM_QUERY_INIT;

	return com_query;
}

void network_mysqld_com_query_result_free(network_mysqld_com_query_result_t *udata) {
	if (!udata) return;

	g_free(udata);
}

int network_mysqld_com_query_result_track_state(network_packet *packet, network_mysqld_com_query_result_t *udata) {
	if (udata->state == PARSE_COM_QUERY_LOAD_DATA) {
		/* if the packet length is 0, the client is done */

		guint32 len = network_mysqld_proto_get_int24(packet);

		if (len == 0) {
			udata->state = PARSE_COM_QUERY_LOAD_DATA_END_DATA;
		}
	}

	return 0;
}

/**
 * @return -1 on error
 *         0  on success and done
 *         1  on success and need more
 */
int network_mysqld_proto_get_com_query_result(network_packet *packet, network_mysqld_com_query_result_t *query) {
	int is_finished = 0;
	gint8 status;

	/**
	 * if we get a OK in the first packet there will be no result-set
	 */
	switch (query->state) {
	case PARSE_COM_QUERY_INIT:
		status = network_mysqld_proto_get_int8(packet);
		packet->offset--; /* only peek at the next byte */

		switch (status) {
		case MYSQLD_PACKET_ERR: /* e.g. SELECT * FROM dual -> ERROR 1096 (HY000): No tables used */
			is_finished = 1;
			break;
		case MYSQLD_PACKET_OK: { /* e.g. DELETE FROM tbl */
			int server_status;
			int warning_count;
			guint64 affected_rows;
			guint64 insert_id;

			network_mysqld_proto_get_ok_packet(packet, &affected_rows, &insert_id, &server_status, &warning_count, NULL);
			if (server_status & SERVER_MORE_RESULTS_EXISTS) {
			
			} else {
				is_finished = 1;
			}

			query->server_status = server_status;
			query->warning_count = warning_count;
			query->affected_rows = affected_rows;
			query->insert_id     = insert_id;
			query->was_resultset = 0;

			break; }
		case MYSQLD_PACKET_NULL:
			/* OH NO, LOAD DATA INFILE :) */
			query->state = PARSE_COM_QUERY_LOAD_DATA;

			is_finished = 1;

			break;
		case MYSQLD_PACKET_EOF:
			g_error("%s.%d: COM_QUERY packet should not be (NULL|EOF), got: %02x",
					__FILE__, __LINE__,
					status);

			break;
		default:
			/* looks like a result */
			query->state = PARSE_COM_QUERY_FIELD;
			break;
		}
		break;
	case PARSE_COM_QUERY_FIELD:
		status = network_mysqld_proto_get_int8(packet);

		switch (status) {
		case MYSQLD_PACKET_ERR:
		case MYSQLD_PACKET_OK:
		case MYSQLD_PACKET_NULL:
			g_error("%s.%d: COM_QUERY should not be (OK|NULL|ERR), got: %02x",
					__FILE__, __LINE__,
					status);

			break;
		case MYSQLD_PACKET_EOF:
#if MYSQL_VERSION_ID >= 50000
			/**
			 * in 5.0 we have CURSORs which have no rows, just a field definition
			 */
			if (packet->data->str[NET_HEADER_SIZE + 3] & SERVER_STATUS_CURSOR_EXISTS) {
				is_finished = 1;
			} else {
				query->state = PARSE_COM_QUERY_RESULT;
			}
#else
			query->state = PARSE_COM_QUERY_RESULT;
#endif
			break;
		default:
			break;
		}
		break;
	case PARSE_COM_QUERY_RESULT:
		status = network_mysqld_proto_get_int8(packet);

		switch (status) {
		case MYSQLD_PACKET_EOF:
			if (packet->data->len == 9) {
				query->warning_count = network_mysqld_proto_get_int16(packet);
				query->server_status = network_mysqld_proto_get_int16(packet);
				query->was_resultset = 1;

				if (query->server_status & SERVER_MORE_RESULTS_EXISTS) {
					query->state = PARSE_COM_QUERY_INIT;
				} else {
					is_finished = 1;
				}
			}

			break;
		case MYSQLD_PACKET_ERR:
			/* like 
			 * 
			 * EXPLAIN SELECT * FROM dual; returns an error
			 * 
			 * EXPLAIN SELECT 1 FROM dual; returns a result-set
			 * */
			is_finished = 1;
			break;
		case MYSQLD_PACKET_OK:
		case MYSQLD_PACKET_NULL: /* the first field might be a NULL */
			break;
		default:
			query->rows++;
			query->bytes += packet->data->len;
			break;
		}
		break;
	case PARSE_COM_QUERY_LOAD_DATA_END_DATA:
		status = network_mysqld_proto_get_int8(packet);

		switch (status) {
		case MYSQLD_PACKET_OK:
			is_finished = 1;
			break;
		case MYSQLD_PACKET_NULL:
		case MYSQLD_PACKET_ERR:
		case MYSQLD_PACKET_EOF:
		default:
			g_error("%s.%d: COM_QUERY,should be (OK), got: %02x",
					__FILE__, __LINE__,
					status);


			break;
		}

		break;
	default:
		g_error("%s.%d: unknown state in COM_QUERY: %d", 
				__FILE__, __LINE__,
				query->state);
	}

	return is_finished;
}


gboolean network_mysqld_com_query_result_is_load_data(network_mysqld_com_query_result_t *udata) {
	return (udata->state == PARSE_COM_QUERY_LOAD_DATA) ? TRUE : FALSE;
}

network_mysqld_com_stmt_prepare_result_t *network_mysqld_com_stmt_prepare_result_new() {
	network_mysqld_com_stmt_prepare_result_t *udata;

	udata = g_new0(network_mysqld_com_stmt_prepare_result_t, 1);
	udata->first_packet = TRUE;

	return udata;
}

int network_mysqld_proto_get_com_stmt_prepare_result(
		network_packet *packet, 
		network_mysqld_com_stmt_prepare_result_t *udata) {
	gint8 status;
	int is_finished = 0;

	status = network_mysqld_proto_get_int8(packet);

	if (udata->first_packet == 1) {
		udata->first_packet = 0;

		switch (status) {
		case MYSQLD_PACKET_OK:
			g_assert(packet->data->len == 12 + NET_HEADER_SIZE); 

			/* the header contains the number of EOFs we expect to see
			 * - no params -> 0
			 * - params | fields -> 1
			 * - params + fields -> 2 
			 */
			udata->want_eofs = 0;

			if (packet->data->str[NET_HEADER_SIZE + 5] != 0 || packet->data->str[NET_HEADER_SIZE + 6] != 0) {
				udata->want_eofs++;
			}
			if (packet->data->str[NET_HEADER_SIZE + 7] != 0 || packet->data->str[NET_HEADER_SIZE + 8] != 0) {
				udata->want_eofs++;
			}

			if (udata->want_eofs == 0) {
				is_finished = 1;
			}

			break;
		case MYSQLD_PACKET_ERR:
			is_finished = 1;
			break;
		default:
			g_error("%s.%d: COM_STMT_PREPARE should either get a (OK|ERR), got %02x",
					__FILE__, __LINE__,
					status);
			break;
		}
	} else {
		switch (status) {
		case MYSQLD_PACKET_OK:
		case MYSQLD_PACKET_NULL:
		case MYSQLD_PACKET_ERR:
			g_error("%s.%d: COM_STMT_PREPARE should not be (OK|ERR|NULL), got: %02x",
					__FILE__, __LINE__,
					status);
			break;
		case MYSQLD_PACKET_EOF:
			if (--udata->want_eofs == 0) {
				is_finished = 1;
			}
			break;
		default:
			break;
		}
	}


	return is_finished;
}



network_mysqld_com_init_db_result_t *network_mysqld_com_init_db_result_new() {
	network_mysqld_com_init_db_result_t *udata;

	udata = g_new0(network_mysqld_com_init_db_result_t, 1);
	udata->db_name = NULL;

	return udata;
}


void network_mysqld_com_init_db_result_free(network_mysqld_com_init_db_result_t *udata) {
	if (udata->db_name) g_string_free(udata->db_name, TRUE);

	g_free(udata);
}

int network_mysqld_com_init_db_result_track_state(network_packet *packet, network_mysqld_com_init_db_result_t *udata) {
	network_mysqld_proto_skip_network_header(packet);
	network_mysqld_proto_skip(packet, 1); /* the command */

	if (packet->offset != packet->data->len) {
		udata->db_name = g_string_new(NULL);

		network_mysqld_proto_get_gstring_len(packet, packet->data->len - packet->offset, udata->db_name);
	} else {
		if (udata->db_name) g_string_free(udata->db_name, TRUE);
		udata->db_name = NULL;
	}

	return 0;
}

int network_mysqld_proto_get_com_init_db(
		network_packet *packet, 
		network_mysqld_com_init_db_result_t *udata,
		network_mysqld_con *con) {
	gint8 status;
	int is_finished;

	/**
	 * in case we have a init-db statement we track the db-change on the server-side
	 * connection
	 */
	status = network_mysqld_proto_get_int8(packet);

	switch (status) {
	case MYSQLD_PACKET_ERR:
		is_finished = 1;
		break;
	case MYSQLD_PACKET_OK:
		/**
		 * track the change of the init_db */
		g_string_truncate(con->server->default_db, 0);
		g_string_truncate(con->client->default_db, 0);

		if (udata->db_name && udata->db_name->len) {
			g_string_append_len(con->server->default_db, 
					S(udata->db_name));
			
			g_string_append_len(con->client->default_db, 
					S(udata->db_name));
		}
		 
		is_finished = 1;
		break;
	default:
		g_critical("%s.%d: COM_INIT_DB should be (ERR|OK), got %02x",
				__FILE__, __LINE__,
				status);

		return -1;
	}

	return is_finished;
}

int network_mysqld_proto_get_query_result(network_packet *packet, network_mysqld_con *con) {
	gint8 status;
	int is_finished = 0;
	
	network_mysqld_proto_skip_network_header(packet);
						
	/* forward the response to the client */
	switch (con->parse.command) {
	case COM_CHANGE_USER: 
		/**
		 * - OK
		 * - ERR (in 5.1.12+ + a duplicate ERR)
		 */
		status = network_mysqld_proto_get_int8(packet);

		switch (status) {
		case MYSQLD_PACKET_ERR:
			is_finished = 1;
			break;
		case MYSQLD_PACKET_OK:
			is_finished = 1;
			break;
		default:
			g_error("%s.%d: COM_(0x%02x) should be (ERR|OK), got %02x",
					__FILE__, __LINE__,
					con->parse.command, status);
			break;
		}
		break;
	case COM_INIT_DB:
		is_finished = network_mysqld_proto_get_com_init_db(packet, con->parse.data, con);

		break;
	case COM_STMT_RESET:
	case COM_PING:
	case COM_PROCESS_KILL:
		status = network_mysqld_proto_get_int8(packet);

		switch (status) {
		case MYSQLD_PACKET_ERR:
		case MYSQLD_PACKET_OK:
			is_finished = 1;
			break;
		default:
			g_error("%s.%d: COM_(0x%02x) should be (ERR|OK), got %02x",
					__FILE__, __LINE__,
					con->parse.command, status);
			break;
		}
		break;
	case COM_DEBUG:
	case COM_SET_OPTION:
	case COM_SHUTDOWN:
		status = network_mysqld_proto_get_int8(packet);

		switch (status) {
		case MYSQLD_PACKET_EOF:
			is_finished = 1;
			break;
		default:
			g_error("%s.%d: COM_(0x%02x) should be EOF, got %02x",
					__FILE__, __LINE__,
					con->parse.command, status);
			break;
		}
		break;

	case COM_FIELD_LIST:
		status = network_mysqld_proto_get_int8(packet);

		/* we transfer some data and wait for the EOF */
		switch (status) {
		case MYSQLD_PACKET_ERR:
		case MYSQLD_PACKET_EOF:
			is_finished = 1;
			break;
		case MYSQLD_PACKET_NULL:
		case MYSQLD_PACKET_OK:
			g_error("%s.%d: COM_(0x%02x) should not be (OK|ERR|NULL), got: %02x",
					__FILE__, __LINE__,
					con->parse.command, status);

			break;
		default:
			break;
		}
		break;
#if MYSQL_VERSION_ID >= 50000
	case COM_STMT_FETCH:
		/*  */
		status = network_mysqld_proto_get_int8(packet);

		switch (status) {
		case MYSQLD_PACKET_EOF:
			if (packet->data->str[NET_HEADER_SIZE + 3] & SERVER_STATUS_LAST_ROW_SENT) {
				is_finished = 1;
			}
			if (packet->data->str[NET_HEADER_SIZE + 3] & SERVER_STATUS_CURSOR_EXISTS) {
				is_finished = 1;
			}
			break;
		default:
			break;
		}
		break;
#endif
	case COM_QUIT: /* sometimes we get a packet before the connection closes */
	case COM_STATISTICS:
		/* just one packet, no EOF */
		is_finished = 1;

		break;
	case COM_STMT_PREPARE:
		is_finished = network_mysqld_proto_get_com_stmt_prepare_result(packet, con->parse.data);
		break;
	case COM_STMT_EXECUTE:
	case COM_QUERY:
		is_finished = network_mysqld_proto_get_com_query_result(packet, con->parse.data);
		break;
	case COM_BINLOG_DUMP:
		/**
		 * the binlog-dump event stops, forward all packets as we see them
		 * and keep the command active
		 */
		is_finished = 1;
		break;
	default:
		g_error("%s.%d: COM_(0x%02x) is not handled", 
				__FILE__, __LINE__,
				con->parse.command);
		break;
	}

	return is_finished;
}

/**
 * parse the result-set packet and extract the fields
 *
 * @param chunk  list of mysql packets 
 * @param fields empty array where the fields shall be stored in
 *
 * @return NULL if there is no resultset
 *         pointer to the chunk after the fields (to the EOF packet)
 */ 
GList *network_mysqld_proto_get_fielddefs(GList *chunk, GPtrArray *fields) {
	network_packet packet;
	guint8 field_count;
	guint i;
    
	/*
	 * read(6, "\1\0\0\1", 4)                  = 4
	 * read(6, "\2", 1)                        = 1
	 * read(6, "6\0\0\2", 4)                   = 4
	 * read(6, "\3def\0\6STATUS\0\rVariable_name\rVariable_name\f\10\0P\0\0\0\375\1\0\0\0\0", 54) = 54
	 * read(6, "&\0\0\3", 4)                   = 4
	 * read(6, "\3def\0\6STATUS\0\5Value\5Value\f\10\0\0\2\0\0\375\1\0\0\0\0", 38) = 38
	 * read(6, "\5\0\0\4", 4)                  = 4
	 * read(6, "\376\0\0\"\0", 5)              = 5
	 * read(6, "\23\0\0\5", 4)                 = 4
	 * read(6, "\17Aborted_clients\00298", 19) = 19
	 *
	 */

	packet.data = chunk->data;
	packet.offset = 0;

	network_mysqld_proto_skip_network_header(&packet);
	
	field_count = network_mysqld_proto_get_int8(&packet); /* the byte after the net-header is the field-count */
    
	/* the next chunk, the field-def */
	for (i = 0; i < field_count; i++) {
		MYSQL_FIELD *field;
        
		chunk = chunk->next;
		g_assert(chunk);

		packet.data = chunk->data;
		packet.offset = 0;

		network_mysqld_proto_skip_network_header(&packet);
 
		field = network_mysqld_proto_fielddef_new();
        
		field->catalog   = network_mysqld_proto_get_lenenc_string(&packet, NULL);
		field->db        = network_mysqld_proto_get_lenenc_string(&packet, NULL);
		field->table     = network_mysqld_proto_get_lenenc_string(&packet, NULL);
		field->org_table = network_mysqld_proto_get_lenenc_string(&packet, NULL);
		field->name      = network_mysqld_proto_get_lenenc_string(&packet, NULL);
		field->org_name  = network_mysqld_proto_get_lenenc_string(&packet, NULL);
        
		network_mysqld_proto_skip(&packet, 1); /* filler */
        
		field->charsetnr = network_mysqld_proto_get_int16(&packet);
		field->length    = network_mysqld_proto_get_int32(&packet);
		field->type      = network_mysqld_proto_get_int8(&packet);
		field->flags     = network_mysqld_proto_get_int16(&packet);
		field->decimals  = network_mysqld_proto_get_int8(&packet);
        
		network_mysqld_proto_skip(&packet, 2); /* filler */
        
		g_ptr_array_add(fields, field);
	}
    
	/* this should be EOF chunk */
	chunk = chunk->next;

	g_assert(chunk);

	packet.data = chunk->data;
	packet.offset = 0;
	
	network_mysqld_proto_skip_network_header(&packet);

	field_count = network_mysqld_proto_get_int8(&packet); /* the byte after the net-header is a EOF */
	g_assert(field_count == (guint8)MYSQLD_PACKET_EOF);
    
	return chunk;
}


