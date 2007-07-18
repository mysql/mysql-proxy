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

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <errno.h>

#include "network-mysqld.h"
#include "network-mysqld-proto.h"

#include "sys-pedantic.h"


/**
 * a simple query handler
 *
 * we handle the basic statements that the mysql-client sends us
 *
 * TODO: we have to split the queries into a basic SQL syntax:
 *
 *   SELECT * 
 *     FROM <table>
 *   [WHERE <field> = <value>]
 *
 * - no joins
 * - no grouping
 *
 *   DELETE
 *     FROM <table>
 * 
 * - no WHERE clause
 *
 * - each table has to have a provider for a table 
 * - each plugin should able to provide tables as needed
 */
int network_mysqld_con_handle_stmt(network_mysqld *srv, network_mysqld_con *con, GString *s) {
	gsize i, j;
	GPtrArray *fields;
	GPtrArray *rows;
	GPtrArray *row;
	gsize packet_len = (s->str[0] << 0) | (s->str[1] << 8) | (s->str[2] << 16);

#define C(x) x, sizeof(x) -1
	
	switch(s->str[NET_HEADER_SIZE]) {
	case COM_QUERY:
		fields = NULL;
		rows = NULL;
		row = NULL;

		if (0 == g_ascii_strncasecmp(s->str + NET_HEADER_SIZE + 1, C("select @@version_comment limit 1"))) {
			MYSQL_FIELD *field;

			fields = network_mysqld_proto_fields_init();

			field = network_mysqld_proto_field_init();
			field->name = g_strdup("@@version_comment");
			field->type = FIELD_TYPE_VAR_STRING;
			g_ptr_array_add(fields, field);

			rows = g_ptr_array_new();
			row = g_ptr_array_new();
			g_ptr_array_add(row, g_strdup("MySQL Enterprise Agent"));
			g_ptr_array_add(rows, row);

			con->client->packet_id++;
			network_mysqld_con_send_resultset(con->client, fields, rows);
			
		} else if (0 == g_ascii_strncasecmp(s->str + NET_HEADER_SIZE + 1, C("select USER()"))) {
			MYSQL_FIELD *field;

			fields = network_mysqld_proto_fields_init();
			field = network_mysqld_proto_field_init();
			field->name = g_strdup("USER()");
			field->type = FIELD_TYPE_VAR_STRING;
			g_ptr_array_add(fields, field);

			rows = g_ptr_array_new();
			row = g_ptr_array_new();
			g_ptr_array_add(row, g_strdup("root"));
			g_ptr_array_add(rows, row);

			con->client->packet_id++;
			network_mysqld_con_send_resultset(con->client, fields, rows);
		} else if (0 == g_ascii_strncasecmp(s->str + NET_HEADER_SIZE + 1, C("update proxy_config set value=1 where option=\"proxy.profiling\""))) {
			srv->config.proxy.profiling = 1;

			con->client->packet_id++;
			network_mysqld_con_send_ok(con->client);
		} else if (0 == g_ascii_strncasecmp(s->str + NET_HEADER_SIZE + 1, C("update proxy_config set value=0 where option=\"proxy.profiling\""))) {
			srv->config.proxy.profiling = 0;
	
			con->client->packet_id++;
			network_mysqld_con_send_ok(con->client);
		} else if (0 == g_ascii_strncasecmp(s->str + NET_HEADER_SIZE + 1, C("stop instance"))) {
			/**
			 * connect to the server via the admin-connection and try to shut down the server
			 * with COM_SHUTDOWN
			 */

			con->client->packet_id++;
			network_mysqld_con_send_ok(con->client);
		} else if (0 == g_ascii_strncasecmp(s->str + NET_HEADER_SIZE + 1, C("start instance"))) {
			/**
			 * start the instance with fork() and monitor it
			 */

			con->client->packet_id++;
			network_mysqld_con_send_ok(con->client);
		} else {
			int have_sent = 0;
			/* check if the table is known */
			if (0 == g_ascii_strncasecmp(s->str + NET_HEADER_SIZE + 1, "select * from ", sizeof("select * from ") - 1)) {
				network_mysqld_table *table;
				GString *table_name = g_string_new(NULL);

				g_string_append_len(table_name, 
						s->str + (NET_HEADER_SIZE + 1 + sizeof("select * from ") - 1),
						s->len - (NET_HEADER_SIZE + 1 + sizeof("select * from ") - 1));

				if ((table = g_hash_table_lookup(srv->tables, table_name->str))) {
					if (table->select) {
						fields = network_mysqld_proto_fields_init();
						rows = g_ptr_array_new();

						table->select(fields, rows, table->user_data);
			
						con->client->packet_id++;
						network_mysqld_con_send_resultset(con->client, fields, rows);

						have_sent = 1;
					} 
				} else {
					g_message("table '%s' not found", table_name->str);
				}
				g_string_free(table_name, TRUE);
			} 
					
			if (!have_sent) {
				con->client->packet_id++;
				network_mysqld_con_send_error(con->client, C("booh"));
			}
		}

		/* clean up */
		if (fields) {
			network_mysqld_proto_fields_free(fields);
			fields = NULL;
		}

		if (rows) {
			for (i = 0; i < rows->len; i++) {
				row = rows->pdata[i];

				for (j = 0; j < row->len; j++) {
					g_free(row->pdata[j]);
				}

				g_ptr_array_free(row, TRUE);
			}
			g_ptr_array_free(rows, TRUE);
			rows = NULL;
		}

		break;
	case COM_QUIT:
		break;
	default:
		con->client->packet_id++;
		network_mysqld_con_send_error(con->client, C("unknown COM_*"));
		break;
	}
#undef C					
	return 0;
}

NETWORK_MYSQLD_PLUGIN_PROTO(server_con_init) {
	const unsigned char handshake[] = 
		"\x0a"  /* protocol version */
		"5.1.20-agent\0" /* version*/
		"\x01\x00\x00\x00" /* 4-byte thread-id */
		"\x3a\x23\x3d\x4b"
		"\x43\x4a\x2e\x43" /* 8-byte scramble buffer */
		"\x00"             /* 1-byte filler */
		"\x00\x02"         /* 2-byte server-cap, we only speak the 4.1 protocol */
		"\x08"             /* 1-byte language */
		"\x02\x00"         /* 2-byte status */
		"\x00\x00\x00\x00" 
		"\x00\x00\x00\x00"
		"\x00\x00\x00\x00"
		"\x00"             /* 13-byte filler */
		;

	network_queue_append(con->client->send_queue, (gchar *)handshake, (sizeof(handshake) - 1), 0);
	
	con->state = CON_STATE_SEND_HANDSHAKE;

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(server_read_auth) {
	GString *s;
	GList *chunk;
	network_socket *recv_sock, *send_sock;
	
	recv_sock = con->client;

	chunk = recv_sock->recv_queue->chunks->tail;
	s = chunk->data;

	if (s->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS; /* we are not finished yet */

	/* the password is fine */
	send_sock = con->client;

	send_sock->packet_id = recv_sock->packet_id + 1;

	network_mysqld_con_send_ok(send_sock);

	g_string_free(chunk->data, TRUE);

	recv_sock->packet_len = PACKET_LEN_UNSET;
	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	con->state = CON_STATE_SEND_AUTH_RESULT;

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(server_read_query) {
	GString *s;
	GList *chunk;
	network_socket *recv_sock, *send_sock;

	recv_sock = con->client;

	chunk = recv_sock->recv_queue->chunks->tail;
	s = chunk->data;

	if (s->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS;
	
	network_mysqld_con_handle_stmt(srv, con, s);
		
	con->parse.len = recv_sock->packet_len;

	g_string_free(chunk->data, TRUE);
	recv_sock->packet_len = PACKET_LEN_UNSET;

	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	con->state = CON_STATE_SEND_QUERY_RESULT;

	return RET_SUCCESS;
}

int network_mysqld_server_connection_init(network_mysqld_con *con) {
	con->plugins.con_init             = server_con_init;

	con->plugins.con_read_auth        = server_read_auth;

	con->plugins.con_read_query       = server_read_query;

	return 0;
}

int network_mysqld_server_init(network_mysqld_con *con) {
	gchar *address = con->config.admin.address;

	if (0 != network_mysqld_con_set_address(&(con->server->addr), address)) {
		g_critical("%s.%d: network_mysqld_con_set_address(%s) failed", __FILE__, __LINE__, con->server->addr.str);
		return -1;
	}
	
	if (0 != network_mysqld_con_bind(con->server)) {
		g_critical("%s.%d: network_mysqld_con_bind(%s) failed", __FILE__, __LINE__, con->server->addr.str);
		return -1;
	}

	return 0;
}


