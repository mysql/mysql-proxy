/* Copyright (C) 2008 MySQL AB */ 

#include <sys/ioctl.h> /* FIONREAD */

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include "network-mysqld.h"
#include "network-mysqld-proto.h"
#include "sys-pedantic.h"


/**
 * we have two phases
 * - getting the binglog-pos with SHOW MASTER STATUS
 * - running the BINLOG_DUMP
 *
 * - split binlog stream into multiple streams based on
 *   lua-script and push the streams into the slaves
 *   - thread-ids
 *   - server-id
 *   - database name
 *   - table-names
 * - rewrite binlogs as delayed streams (listening port per delay)
 *
 * - chaining of replicants is desired
 *   a delayed replicator can feed a splitter or the other way around
 *
 * - we have to maintain the last know position per backend, perhaps 
 *   we want to maintain this in lua land and use the tbl2str functions
 *
 * - we may want to share the config
 *
 * - we have to parse the binlog stream and should also provide a 
 *   binlog reading library
 *
 *
 */
typedef struct {
	enum { REPCLIENT_BINLOG_GET_POS, REPCLIENT_BINLOG_DUMP } state;
	char *binlog_file;
	int binlog_pos;
} plugin_con_state;

struct chassis_plugin_config {
	gchar *master_address;                   /**< listening address of the proxy */

	network_mysqld_con *listen_con;
};


static plugin_con_state *plugin_con_state_init() {
	plugin_con_state *st;

	st = g_new0(plugin_con_state, 1);

	return st;
}

static void plugin_con_state_free(plugin_con_state *st) {
	if (!st) return;

	if (st->binlog_file) g_free(st->binlog_file);

	g_free(st);
}

/**
 * decode the result-set of SHOW MASTER STATUS
 */
static int network_mysqld_resultset_master_status(chassis *UNUSED_PARAM(srv), network_mysqld_con *con) {
	int field_count;
	GList *chunk;
	GString *s;
	int i;
	network_socket *sock = con->client;
	plugin_con_state *st = con->plugin_con_state;

	/* scan the resultset */
	chunk = sock->send_queue->chunks->head;
	s = chunk->data;

	/* the first chunk is the length
	 *  */
	if (s->len != NET_HEADER_SIZE + 1) {
		write(STDERR_FILENO, s->str, s->len);
		g_error("%s.%d: "F_SIZE_T" != 5", 
				__FILE__, __LINE__,
				s->len);
	}
	
	field_count = s->str[NET_HEADER_SIZE]; /* the byte after the net-header is the field-count */

	g_assert(field_count > 2); /* we need at least File and Position */

	/* the next chunk, the field-def */
	for (i = 0; i < field_count; i++) {
		/* skip the field def */
		chunk = chunk->next;
	}

	/* this should be EOF chunk */
	chunk = chunk->next;
	s = chunk->data;
	
	g_assert(s->str[NET_HEADER_SIZE] == MYSQLD_PACKET_EOF);

	/* a data row */
	while (NULL != (chunk = chunk->next)) {
		guint32 off = 4;
		
		s = chunk->data;

		/* if we find the 2nd EOF packet we are done */
		if (s->str[off] == MYSQLD_PACKET_EOF &&
		    s->len < 10) break;

		for (i = 0; i < field_count; i++) {
			guint64 field_len = network_mysqld_proto_get_lenenc_int(s, &off);

			g_assert(off <= s->len);

			if (i == 0) {
				g_assert(field_len > 0);
				/* Position */
				
				if (st->binlog_file) g_free(st->binlog_file);

				st->binlog_file = g_strndup(s->str + off, field_len);
			} else if (i == 1) {
				/* is a string */
				guint64 j;
				g_assert(field_len > 0);

				st->binlog_pos = 0;

				for (j = 0; j < field_len; j++) {
					st->binlog_pos *= 10;
					st->binlog_pos += s->str[off + j] - '0';
				}

			}

			off += field_len;
		}

		g_message("reading binlog from: binlog-file: %s, binlog-pos: %d", 
				st->binlog_file, st->binlog_pos);
	}

	return 0;
}

NETWORK_MYSQLD_PLUGIN_PROTO(repclient_read_handshake) {
	GString *s;
	GList *chunk;
	network_socket *recv_sock;
	network_socket *send_sock = NULL;
	const char auth[] = 
		"\x85\xa6\x03\x00" /** client flags */
		"\x00\x00\x00\x01" /** max packet size */
		"\x08"             /** charset */
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00\x00\x00"     /** fillers */
		"repl\x00"         /** nul-term username */
#if 0
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00" /** scramble buf */
#endif
		"\x00"
#if 0
		"\x00"             /** nul-term db-name */
#endif
		;
	
	recv_sock = con->server;
	send_sock = con->server;

	chunk = recv_sock->recv_queue->chunks->tail;
	s = chunk->data;

	if (s->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS;

	g_string_free(chunk->data, TRUE);
	recv_sock->packet_len = PACKET_LEN_UNSET;

	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	/* we have to reply with a useful password */
	network_queue_append(send_sock->send_queue, auth, sizeof(auth) - 1, send_sock->packet_id + 1);

	con->state = CON_STATE_SEND_AUTH;

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(repclient_read_auth_result) {
	GString *packet;
	GList *chunk;
	network_socket *recv_sock;
	network_socket *send_sock = NULL;

	const char query_packet[] = 
		"\x03"                    /* COM_QUERY */
		"SHOW MASTER STATUS"
		;

	recv_sock = con->server;

	chunk = recv_sock->recv_queue->chunks->tail;
	packet = chunk->data;

	/* we aren't finished yet */
	if (packet->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS;

	/* the auth should be fine */
	switch (packet->str[NET_HEADER_SIZE + 0]) {
	case MYSQLD_PACKET_ERR:
		g_assert(packet->str[NET_HEADER_SIZE + 3] == '#');

		g_critical("%s.%d: error: %d", 
				__FILE__, __LINE__,
				packet->str[NET_HEADER_SIZE + 1] | (packet->str[NET_HEADER_SIZE + 2] << 8));

		return RET_ERROR;
	case MYSQLD_PACKET_OK: 
		break; 
	default:
		g_error("%s.%d: packet should be (OK|ERR), got: %02x",
				__FILE__, __LINE__,
				packet->str[NET_HEADER_SIZE + 0]);

		return RET_ERROR;
	} 

	g_string_free(chunk->data, TRUE);
	recv_sock->packet_len = PACKET_LEN_UNSET;

	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	send_sock = con->server;
	network_queue_append(send_sock->send_queue, query_packet, sizeof(query_packet) - 1, 0);

	con->state = CON_STATE_SEND_QUERY;

	return RET_SUCCESS;
}

/**
 * inject a COM_BINLOG_DUMP after we have sent our SHOW MASTER STATUS
 */
NETWORK_MYSQLD_PLUGIN_PROTO(repclient_read_query_result) {
	/* let's send the
	 *
	 * ask the server for the current binlog-file|pos and dump everything from there
	 *  
	 * - COM_BINLOG_DUMP
	 *   - 4byte pos
	 *   - 2byte flags (BINLOG_DUMP_NON_BLOCK)
	 *   - 4byte slave-server-id
	 *   - nul-term binlog name
	 *
	 * we don't need:
	 * - COM_REGISTER_SLAVE
	 *   - 4byte server-id
	 *   - nul-term host
	 *   - nul-term user
	 *   - nul-term password
	 *   - 2byte port
	 *   - 4byte recovery rank
	 *   - 4byte master-id
	 */
	GString *packet;
	GList *chunk;
	network_socket *recv_sock, *send_sock;
	int is_finished = 0;
	plugin_con_state *st = con->plugin_con_state;

	recv_sock = con->server;
	send_sock = con->client;

	chunk = recv_sock->recv_queue->chunks->tail;
	packet = chunk->data;

	if (packet->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS; /* packet isn't finished yet */
#if 0
	g_message("%s.%d: packet-len: %08x, packet-id: %d, command: COM_(%02x)", 
			__FILE__, __LINE__,
			recv_sock->packet_len,
			recv_sock->packet_id,
			con->parse.command
		);
#endif						

	switch (con->parse.command) {
	case COM_QUERY:
		/**
		 * if we get a OK in the first packet there will be no result-set
		 */
		switch (con->parse.state.query) {
		case PARSE_COM_QUERY_INIT:
			switch (packet->str[NET_HEADER_SIZE]) {
			case MYSQLD_PACKET_ERR: /* e.g. SELECT * FROM dual -> ERROR 1096 (HY000): No tables used */
				g_assert(con->parse.state.query == PARSE_COM_QUERY_INIT);

				is_finished = 1;
				break;
			case MYSQLD_PACKET_OK: { /* e.g. DELETE FROM tbl */
				int server_status;
				GString s;

				s.str = packet->str + NET_HEADER_SIZE;
				s.len = packet->len - NET_HEADER_SIZE;

				network_mysqld_proto_get_ok_packet(&s, NULL, NULL, &server_status, NULL, NULL);
				if (server_status & SERVER_MORE_RESULTS_EXISTS) {
				
				} else {
					is_finished = 1;
				}
				break; }
			case MYSQLD_PACKET_NULL:
				/* OH NO, LOAD DATA INFILE :) */
				con->parse.state.query = PARSE_COM_QUERY_LOAD_DATA;

				is_finished = 1;

				break;
			case MYSQLD_PACKET_EOF:
				g_error("%s.%d: COM_(0x%02x), packet %d should not be (NULL|EOF), got: %02x",
						__FILE__, __LINE__,
						con->parse.command, recv_sock->packet_id, packet->str[NET_HEADER_SIZE + 0]);

				break;
			default:
				/* looks like a result */
				con->parse.state.query = PARSE_COM_QUERY_FIELD;
				break;
			}
			break;
		case PARSE_COM_QUERY_FIELD:
			switch (packet->str[NET_HEADER_SIZE + 0]) {
			case MYSQLD_PACKET_ERR:
			case MYSQLD_PACKET_OK:
			case MYSQLD_PACKET_NULL:
				g_error("%s.%d: COM_(0x%02x), packet %d should not be (OK|NULL|ERR), got: %02x",
						__FILE__, __LINE__,
						con->parse.command, recv_sock->packet_id, packet->str[NET_HEADER_SIZE + 0]);

				break;
			case MYSQLD_PACKET_EOF:
				if (packet->str[NET_HEADER_SIZE + 3] & SERVER_STATUS_CURSOR_EXISTS) {
					is_finished = 1;
				} else {
					con->parse.state.query = PARSE_COM_QUERY_RESULT;
				}
				break;
			default:
				break;
			}
			break;
		case PARSE_COM_QUERY_RESULT:
			switch (packet->str[NET_HEADER_SIZE + 0]) {
			case MYSQLD_PACKET_EOF:
				if (recv_sock->packet_len < 9) {
					/* so much on the binary-length-encoding 
					 *
					 * sometimes the len-encoding is ...
					 *
					 * */

					if (packet->str[NET_HEADER_SIZE + 3] & SERVER_MORE_RESULTS_EXISTS) {
						con->parse.state.query = PARSE_COM_QUERY_INIT;
					} else {
						is_finished = 1;
					}
				}

				break;
			case MYSQLD_PACKET_ERR:
			case MYSQLD_PACKET_OK:
			case MYSQLD_PACKET_NULL: /* the first field might be a NULL */
				break;
			default:
				break;
			}
			break;
		case PARSE_COM_QUERY_LOAD_DATA_END_DATA:
			switch (packet->str[NET_HEADER_SIZE + 0]) {
			case MYSQLD_PACKET_OK:
				is_finished = 1;
				break;
			case MYSQLD_PACKET_NULL:
			case MYSQLD_PACKET_ERR:
			case MYSQLD_PACKET_EOF:
			default:
				g_error("%s.%d: COM_(0x%02x), packet %d should be (OK), got: %02x",
						__FILE__, __LINE__,
						con->parse.command, recv_sock->packet_id, packet->str[NET_HEADER_SIZE + 0]);


				break;
			}

			break;
		default:
			g_error("%s.%d: unknown state in COM_(0x%02x): %d", 
					__FILE__, __LINE__,
					con->parse.command,
					con->parse.state.query);
		}
		break;

	case COM_BINLOG_DUMP:
		switch (packet->str[NET_HEADER_SIZE + 0]) {
		case MYSQLD_PACKET_ERR:
			g_error("%s.%d: ERR in COM_(0x%02x): %d", 
					__FILE__, __LINE__,
					con->parse.command,
					(unsigned char)packet->str[NET_HEADER_SIZE + 1] | ((unsigned char)packet->str[NET_HEADER_SIZE + 2] << 8));

			break;
		case MYSQLD_PACKET_OK: {
			struct {
				guint32 timestamp;
				enum Log_event_type event_type;
				guint32 server_id;
				guint32 event_size;
				guint32 log_pos;
				guint16 flags;
			} binlog;
			guint off;
			
			/**
			 * decoding the binlog packet
			 *
			 * - http://dev.mysql.com/doc/internals/en/replication-common-header.html
			 *
			 * /\0\0\1
			 *   \0         - OK
			 *     \0\0\0\0 - timestamp
			 *     \4       - ROTATE
			 *     \1\0\0\0 - server-id
			 *     .\0\0\0  - event-size
			 *     \0\0\0\0 - log-pos
			 *     \0\0     - flags
			 *     f\0\0\0\0\0\0\0hostname-bin.000009
			 * c\0\0\2
			 *   \0 
			 *     F\335\6F - timestamp
			 *     \17      - FORMAT_DESCRIPTION_EVENT
			 *     \1\0\0\0 - server-id
			 *     b\0\0\0  - event-size
			 *     \0\0\0\0 - log-pos
			 *     \0\0     - flags
			 *     \4\0005.1.16-beta-log\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0238\r\0\10\0\22\0\4\4\4\4\22\0\0O\0\4\32\10\10\10\10
			 * N\0\0\3
			 *   \0          
			 *     g\255\7F   - timestamp
			 *     \2         - QUERY_EVENT
			 *     \1\0\0\0   - server-id
			 *     M\0\0\0    - event-size
			 *     \263\0\0\0 - log-pos
			 *     \20\0      - flags
			 *       \2\0\0\0 - thread-id
			 *       \0\0\0\0 - query-time
			 *       \5       - str-len of default-db (world)
			 *       \0\0     - error-code on master-side
			 *         \32\0  - var-size-len (5.0 and later)
			 *           \0   - Q_FLAGS2_CODE
			 *             \0@\0\0 - flags (4byte)
			 *           \1   - Q_SQL_MODE_CODE
			 *             \0\0\0\0\0\0\0\0 (8byte)
			 *           \6   - Q_CATALOG_NZ_CODE
			 *             \3std (4byte)
			 *           \4   - Q_CHARSET_CODE
			 *             \10\0\10\0\10\0 (6byte)
			 *           world\0 - 
			 *           drop table t1
			 * Y\0\0\4
			 *   \0 
			 *     \261\255\7F
			 *     \2
			 *     \1\0\0\0
			 *     X\0\0\0
			 *     \v\1...
			 *
			 */


			network_mysqld_proto_skip(packet, &off, NET_HEADER_SIZE + 1);

			binlog.timestamp  = network_mysqld_proto_get_int32(packet, &off);
			binlog.event_type = network_mysqld_proto_get_int8(packet, &off);
			binlog.server_id  = network_mysqld_proto_get_int32(packet, &off);
			binlog.event_size = network_mysqld_proto_get_int32(packet, &off);
			binlog.log_pos    = network_mysqld_proto_get_int32(packet, &off);
			binlog.flags      = network_mysqld_proto_get_int16(packet, &off);


#if 0
			g_message("%s.%d: timestamp = %u, event_type = %02x, server_id = %u, event-size = %u, log-pos = %u, flags = %04x", 
					__FILE__, __LINE__,
					binlog.timestamp, 
					binlog.event_type,
					binlog.server_id,
					binlog.event_size,
					binlog.log_pos,
									binlog.flags);
#endif

			switch (binlog.event_type) {
			case QUERY_EVENT: {
				struct {
					guint32 thread_id;
					guint32 exec_time;
					guint8  db_name_len;
					guint16 error_code;
				} query_event;
				
				guint16 var_size = 0;
				gchar *query_str = NULL;
				
				query_event.thread_id   = network_mysqld_proto_get_int32(packet, &off);
				query_event.exec_time   = network_mysqld_proto_get_int32(packet, &off);
				query_event.db_name_len = network_mysqld_proto_get_int8(packet, &off);
				query_event.error_code  = network_mysqld_proto_get_int16(packet, &off);

				/* 5.0 has more flags */

				var_size    = network_mysqld_proto_get_int16(packet, &off);

				network_mysqld_proto_skip(packet, &off, var_size);

				/* default db has <db_name_len> chars */

				network_mysqld_proto_skip(packet, &off, query_event.db_name_len);
				network_mysqld_proto_skip(packet, &off, 1); /* the \0 */

				query_str = g_strndup(packet->str + off, recv_sock->packet_len - off + NET_HEADER_SIZE);
#define CONST_STR_LEN(x) x, sizeof(x) - 1
				/* parse the query string if it is a DDL statement */
				if (0 == g_ascii_strncasecmp(query_str, CONST_STR_LEN("drop table ")) ||
				    0 == g_ascii_strncasecmp(query_str, CONST_STR_LEN("create table ")) ||
				    0 == g_ascii_strncasecmp(query_str, CONST_STR_LEN("alter table ")) ||
				    0 == g_ascii_strncasecmp(query_str, CONST_STR_LEN("create database ")) ||
				    0 == g_ascii_strncasecmp(query_str, CONST_STR_LEN("alter database ")) ||
				    0 == g_ascii_strncasecmp(query_str, CONST_STR_LEN("drop database ")) ||
				    0 == g_ascii_strncasecmp(query_str, CONST_STR_LEN("create index ")) ||
				    0 == g_ascii_strncasecmp(query_str, CONST_STR_LEN("drop index "))) {
#if 1
					g_message("(DDL) thread-id = %u, exec_time = %u, error-code = %04x\n  QUERY: %s", 
							query_event.thread_id,
							query_event.exec_time,
							query_event.error_code,
							query_str);
#endif

				}

				g_free(query_str);

				
				break; }
			default:
				break;
			}

			/* looks like the binlog dump started */
			is_finished = 1;

			break; }
		case MYSQLD_PACKET_EOF:
			/* EOF signals a NON-blocking read
			 *
			 * we have to send a BINLOG_DUMP from the last position to start the dump again
			 *  */
			is_finished = 1;

			break;

		default:
			g_error("%s.%d: COM_(0x%02x), packet %d should be (OK|ERR|EOF), got: %02x",
						__FILE__, __LINE__,
						con->parse.command, recv_sock->packet_id, packet->str[NET_HEADER_SIZE + 0]);
		}
		break;
	default:
		g_error("%s.%d: COM_(0x%02x) is not handled", 
				__FILE__, __LINE__,
				con->parse.command);
		break;
	}

	network_queue_append(send_sock->send_queue, packet->str + NET_HEADER_SIZE, packet->len - NET_HEADER_SIZE, recv_sock->packet_id);

	/* ... */
	if (is_finished) {
		/**
		 * the resultset handler might decide to trash the send-queue
		 * 
		 * */
		GString *query_packet;
		int my_server_id = 2;

		switch (st->state) {
		case REPCLIENT_BINLOG_GET_POS:
			/* parse the result-set and get the 1st and 2nd column */

			network_mysqld_resultset_master_status(chas, con);

			/* remove all packets */
			while ((packet = g_queue_pop_head(send_sock->send_queue->chunks))) g_string_free(packet, TRUE);

			st->state = REPCLIENT_BINLOG_DUMP;

			query_packet = g_string_new(NULL);

			g_string_append_c(query_packet, '\x12'); /** COM_BINLOG_DUMP */
			g_string_append_c(query_packet, (st->binlog_pos >>  0) & 0xff); /* position */
			g_string_append_c(query_packet, (st->binlog_pos >>  8) & 0xff);
			g_string_append_c(query_packet, (st->binlog_pos >> 16) & 0xff);
			g_string_append_c(query_packet, (st->binlog_pos >> 24) & 0xff);
			g_string_append_c(query_packet, '\x00'); /* flags */
			g_string_append_c(query_packet, '\x00');
			g_string_append_c(query_packet, (my_server_id >>  0) & 0xff); /* server-id */
			g_string_append_c(query_packet, (my_server_id >>  8) & 0xff);
			g_string_append_c(query_packet, (my_server_id >> 16) & 0xff);
			g_string_append_c(query_packet, (my_server_id >> 24) & 0xff);
			g_string_append(query_packet, st->binlog_file); /* filename */
			g_string_append_c(query_packet, '\x00');
		       	
			send_sock = con->server;
			network_queue_append(send_sock->send_queue, query_packet->str, query_packet->len, 0);
		
			g_string_free(query_packet, TRUE);
		
			con->state = CON_STATE_SEND_QUERY;

			break;
		case REPCLIENT_BINLOG_DUMP:
			/* remove all packets */

			/* trash the packets for the injection query */
			while ((packet = g_queue_pop_head(send_sock->send_queue->chunks))) g_string_free(packet, TRUE);

			con->state = CON_STATE_READ_QUERY_RESULT;
			break;
		}
	}

	if (chunk->data) g_string_free(chunk->data, TRUE);
	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	recv_sock->packet_len = PACKET_LEN_UNSET;

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(repclient_connect_server) {
	chassis_plugin_config *config = con->config;
	gchar *address = config->master_address;

	con->server = network_socket_init();

	if (0 != network_mysqld_con_set_address(&(con->server->addr), address)) {
		return -1;
	}

	if (0 != network_mysqld_con_connect(con->server)) {
		return -1;
	}

	fcntl(con->server->fd, F_SETFL, O_NONBLOCK | O_RDWR);

	con->state = CON_STATE_SEND_HANDSHAKE;

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(repclient_init) {
	g_assert(con->plugin_con_state == NULL);

	con->plugin_con_state = plugin_con_state_init();
	
	con->state = CON_STATE_CONNECT_SERVER;

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(repclient_cleanup) {
	if (con->plugin_con_state == NULL) return RET_SUCCESS;

	plugin_con_state_free(con->plugin_con_state);
	
	con->plugin_con_state = NULL;

	return RET_SUCCESS;
}

int network_mysqld_repclient_connection_init(chassis *srv, network_mysqld_con *con) {
	con->plugins.con_init                      = repclient_init;
	con->plugins.con_connect_server            = repclient_connect_server;
	con->plugins.con_read_handshake            = repclient_read_handshake;
	con->plugins.con_read_auth_result          = repclient_read_auth_result;
	con->plugins.con_read_query_result         = repclient_read_query_result;
	con->plugins.con_cleanup                   = repclient_cleanup;

	return 0;
}

/**
 * free the global scope which is shared between all connections
 *
 * make sure that is called after all connections are closed
 */
void network_mysqld_replicant_free(network_mysqld_con *con) {
}

chassis_plugin_config * network_mysqld_replicant_plugin_init(void) {
	chassis_plugin_config *config;

	config = g_new0(chassis_plugin_config, 1);

	return config;
}

void network_mysqld_replicant_plugin_free(chassis_plugin_config *config) {
	gsize i;

	if (config->listen_con) {
		/**
		 * the connection will be free()ed by the network_mysqld_free()
		 */
#if 0
		event_del(&(config->listen_con->server->event));
		network_mysqld_con_free(config->listen_con);
#endif
	}

	if (config->master_address) {
		/* free the global scope */
		g_free(config->master_address);
	}

	g_free(config);
}

/**
 * plugin options 
 */
static GOptionEntry * network_mysqld_replicant_plugin_get_options(chassis_plugin_config *config) {
	guint i;

	/* make sure it isn't collected */
	static GOptionEntry config_entries[] = 
	{
		{ "replicant-master-address",            0, 0, G_OPTION_ARG_STRING, NULL, "... (default: :4040)", "<host:port>" },
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	i = 0;
	config_entries[i++].arg_data = &(config->master_address);
	
	return config_entries;
}

/**
 * init the plugin with the parsed config
 */
int network_mysqld_replicant_plugin_apply_config(chassis *chas, chassis_plugin_config *config) {
	network_mysqld_con *con;
	network_socket *listen_sock;

	if (!config->master_address) config->master_address = g_strdup(":4040");

	return 0;
}

int plugin_init(chassis_plugin *p) {
	/* append the our init function to the init-hook-list */

	p->init         = network_mysqld_replicant_plugin_init;
	p->get_options  = network_mysqld_replicant_plugin_get_options;
	p->apply_config = network_mysqld_replicant_plugin_apply_config;
	p->destroy      = network_mysqld_replicant_plugin_free;

	return 0;
}

