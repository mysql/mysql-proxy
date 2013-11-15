/* $%BEGINLICENSE%$
 Copyright (c) 2008, 2013, Oracle and/or its affiliates. All rights reserved.

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
 

#include <sys/ioctl.h> /* FIONREAD */

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include "network-mysqld.h"
#include "chassis-plugin.h"
#include "network-mysqld-proto.h"
#include "sys-pedantic.h"

/* readline.h doesn't include stdio.h on darwin - Apple bug #5704686 */
#include <stdio.h>
#include <readline/readline.h>

#ifndef PLUGIN_VERSION
#ifdef CHASSIS_BUILD_TAG
#define PLUGIN_VERSION PACKAGE_VERSION "." CHASSIS_BUILD_TAG
#else
#define PLUGIN_VERSION PACKAGE_VERSION
#endif
#endif

typedef struct {
	gchar *command;
} plugin_con_state;

struct chassis_plugin_config {
	gchar *mysqld_username; /**< login-name */
	gchar *mysqld_password; /**< password */
	gchar *mysqld_address;  /**< address (unix-socket or ipv4) */

	network_mysqld_con *con;
};

static plugin_con_state *plugin_con_state_init() {
	plugin_con_state *st;

	st = g_new0(plugin_con_state, 1);

	return st;
}

static void plugin_con_state_free(plugin_con_state *st) {
	if (!st) return;

	if (st->command) g_free(st->command);

	g_free(st);
}

/**
 * called after we received the hand-shake from the
 * server 
 *
 * we intercept the hand-shake packet and send a 
 * client-auth packet back. 
 *
 * @fixme: add scramble + sha1() support
 */
NETWORK_MYSQLD_PLUGIN_PROTO(cli_read_handshake) {
	GString *s;
	GList *chunk;
	network_socket *recv_sock;
	network_socket *send_sock = NULL;
	chassis_plugin_config *config = con->config;
	int i;

	GString *auth = g_string_new(NULL);
	
	network_mysqld_proto_append_int8(auth, 0x85);
	network_mysqld_proto_append_int8(auth, 0xa6);
	network_mysqld_proto_append_int8(auth, 0x03);
	network_mysqld_proto_append_int8(auth, 0x00);

	network_mysqld_proto_append_int32(auth, 16 * 1024 * 1024); /* max-allowed-packet */
	
	network_mysqld_proto_append_int8(auth, 0x08); /* charset */

	for (i = 0; i < 23; i++) { /* filler */
		network_mysqld_proto_append_int8(auth, 0x00);
	}

	g_string_append(auth, config->mysqld_username);
	network_mysqld_proto_append_int8(auth, 0x00);
#if 0
	for (i = 0; i < 8; i++) { /* scramble buf */
		network_mysqld_proto_append_int8(auth, 0x00);
	}
#endif

	/* no scramble, no default-db */
	network_mysqld_proto_append_int8(auth, 0x00);
#if 0
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
#endif	
	recv_sock = con->server;
	send_sock = con->server;

	chunk = recv_sock->recv_queue->chunks->tail;
	s = chunk->data;

	if (s->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS;

	g_string_free(chunk->data, TRUE);
	recv_sock->packet_len = PACKET_LEN_UNSET;

	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	/* we have to reply with a useful password */
	network_queue_append(send_sock->send_queue, auth->str, auth->len, send_sock->packet_id + 1);

	con->state = CON_STATE_SEND_AUTH;

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(cli_read_auth_result) {
	GString *packet;
	GList *chunk;
	network_socket *recv_sock;
	network_socket *send_sock = NULL;

	const char query_packet[] = 
		"\x03"                    /* COM_QUERY */
		"SELECT 1"
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

		g_critical("%s: cli_read_auth_result: auth failed: errno=%d, %s", 
				G_STRLOC,
				packet->str[NET_HEADER_SIZE + 1] | (packet->str[NET_HEADER_SIZE + 2] << 8),
				packet->str + NET_HEADER_SIZE + 9);

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
NETWORK_MYSQLD_PLUGIN_PROTO(cli_read_query_result) {
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
	default:
		g_error("%s.%d: COM_(0x%02x) is not handled", 
				__FILE__, __LINE__,
				con->parse.command);
		break;
	}

	if (chunk->data) g_string_free(chunk->data, TRUE);
	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	recv_sock->packet_len = PACKET_LEN_UNSET;

	/* ... */
	if (is_finished) {
		/**
		 * the resultset handler might decide to trash the send-queue
		 * 
		 * */
		GString *query_packet;
		gchar *line;

		query_packet = g_string_new(NULL);

		g_string_append_c(query_packet, '\x03'); /** COM_BINLOG_DUMP */

		line = readline("cli> ");
		if (line) {
			g_string_append(query_packet, line);
			free(line);
	
			network_queue_append(recv_sock->send_queue, query_packet->str, query_packet->len, 0);
	
			g_string_free(query_packet, TRUE);
	
			con->state = CON_STATE_SEND_QUERY;
		} else {
			chassis_set_shutdown();

			con->state = CON_STATE_ERROR;
		}
	}

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(cli_connect_server) {
	chassis_plugin_config *config = con->config;
	gchar *address = config->mysqld_address;

	if (con->server) {
		int so_error = 0;
		socklen_t so_error_len = sizeof(so_error);

		/**
		 * we might get called a 2nd time after a connect() == EINPROGRESS
		 */
		if (getsockopt(con->server->fd, SOL_SOCKET, SO_ERROR, &so_error, &so_error_len)) {
			/* getsockopt failed */
			g_critical("%s.%d: getsockopt(%s) failed: %s", 
					__FILE__, __LINE__,
					con->server->addr.str, g_strerror(errno));
			return RET_ERROR;
		}

		switch (so_error) {
		case 0:
			break;
		default:
			g_critical("%s.%d: connect(%s) failed: %s", 
					__FILE__, __LINE__,
					con->server->addr.str, g_strerror(so_error));
			return RET_ERROR;
		}
	} else {
		con->server = network_socket_init();
	
		if (0 != network_mysqld_con_set_address(&(con->server->addr), address)) {
			return RET_ERROR;
		}
	
		switch(network_mysqld_con_connect(con->server)) {
		case -2:
			/* the socket is non-blocking already, 
			 * call getsockopt() to see if we are done */
			return RET_ERROR_RETRY;
		case 0:
			break;
		default:
			g_message("%s.%d: connecting to backend (%s) failed, marking it as down for ...", 
					__FILE__, __LINE__, con->server->addr.str);
	
			return RET_ERROR;
		}
	}

	/* we are good, send our handshake and try to login */
	con->state = CON_STATE_READ_HANDSHAKE;

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(cli_init) {
	g_assert(con->plugin_con_state == NULL);

	con->plugin_con_state = plugin_con_state_init();
	
	con->state = CON_STATE_CONNECT_SERVER;

	return RET_SUCCESS;
}

NETWORK_MYSQLD_PLUGIN_PROTO(cli_cleanup) {
	if (con->plugin_con_state == NULL) return RET_SUCCESS;

	plugin_con_state_free(con->plugin_con_state);
	
	con->plugin_con_state = NULL;

	return RET_SUCCESS;
}

static int network_mysqld_cli_connection_init(chassis *chas, network_mysqld_con *con) {
	con->plugins.con_init                      = cli_init;
	con->plugins.con_connect_server            = cli_connect_server;
	con->plugins.con_read_handshake            = cli_read_handshake;
	con->plugins.con_read_auth_result          = cli_read_auth_result;
	con->plugins.con_read_query_result         = cli_read_query_result;
	con->plugins.con_cleanup                   = cli_cleanup;

	return 0;
}

/**
 * free the global scope which is shared between all connections
 *
 * make sure that is called after all connections are closed
 */
void network_mysqld_cli_free(network_mysqld_con *con) {
}

chassis_plugin_config * network_mysqld_cli_plugin_init(void) {
	chassis_plugin_config *config;

	config = g_new0(chassis_plugin_config, 1);

	return config;
}

void network_mysqld_cli_plugin_free(chassis_plugin_config *config) {
	if (config->mysqld_address) {
		/* free the global scope */
		g_free(config->mysqld_username);
		g_free(config->mysqld_password);
		g_free(config->mysqld_address);
	}

	g_free(config);
}

/**
 * plugin options 
 */
static GOptionEntry * network_mysqld_cli_plugin_get_options(chassis_plugin_config *config) {
	guint i;

	/* make sure it isn't collected */
	static GOptionEntry config_entries[] = 
	{
		{ "address",            0, 0, G_OPTION_ARG_STRING, NULL, "... (default: 127.0.0.1:3306)", "<host:port>" },
		{ "username",           0, 0, G_OPTION_ARG_STRING, NULL, "... (default: mysql)", "username" },
		{ "password",           0, 0, G_OPTION_ARG_STRING, NULL, "... (default: )", "password" },
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	i = 0;
	config_entries[i++].arg_data = &(config->mysqld_address);
	config_entries[i++].arg_data = &(config->mysqld_username);
	config_entries[i++].arg_data = &(config->mysqld_password);
	
	return config_entries;
}

/**
 * init the plugin with the parsed config
 */
int network_mysqld_cli_plugin_apply_config(chassis *chas, chassis_plugin_config *config) {
	network_mysqld_con *con;
	network_socket *listen_sock;

	if (!config->mysqld_address) config->mysqld_address = g_strdup("127.0.0.1:3306");
	if (!config->mysqld_username) config->mysqld_username = g_strdup("mysql");
	if (!config->mysqld_password) config->mysqld_password = g_strdup("");

	/**
	 * we don't really need this connection handle
	 * but it carries our shared config 
	 */
	con = network_mysqld_con_init();
	network_mysqld_add_connection(chas, con);
	con->config = config;

	network_mysqld_cli_connection_init(chas, con);
	
	network_mysqld_con_handle(-1, 0, con);

	return 0;
}

int plugin_init(chassis_plugin *p) {
	/* append the our init function to the init-hook-list */
	p->magic        = CHASSIS_PLUGIN_MAGIC;
	p->name         = g_strdup("client");
	p->version		= g_strdup(PLUGIN_VERSION);

	p->init         = network_mysqld_cli_plugin_init;
	p->get_options  = network_mysqld_cli_plugin_get_options;
	p->apply_config = network_mysqld_cli_plugin_apply_config;
	p->destroy      = network_mysqld_cli_plugin_free;

	return 0;
}

