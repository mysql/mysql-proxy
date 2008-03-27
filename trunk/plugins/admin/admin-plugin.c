/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

#include <errno.h>

#include "network-mysqld.h"
#include "network-mysqld-proto.h"

#include "sys-pedantic.h"

#include <gmodule.h>

/**
 * a simple query handler
 *
 * we handle the basic statements that the mysql-client sends us
 *
 * @todo we have to split the queries into a basic SQL syntax:
 *
 *   SELECT * 
 *     FROM table
 *   [WHERE field = value]
 *
 * - no joins
 * - no grouping
 *
 *   DELETE
 *     FROM table
 * 
 * - no WHERE clause
 *
 * - each table has to have a provider for a table 
 * - each plugin should able to provide tables as needed
 */

struct chassis_plugin_config {
	gchar *address;                   /**< listening address of the admin interface */

	gchar *lua_script;                /**< script to load at the start the connection */

	network_mysqld_con *listen_con;
};

int network_mysqld_con_handle_stmt(chassis *srv, network_mysqld_con *con, GString *s) {
	gsize i, j;
	GPtrArray *fields;
	GPtrArray *rows;
	GPtrArray *row;

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
		} else {
			con->client->packet_id++;
			network_mysqld_con_send_error(con->client, C("(admin-server) query not known"));
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
	case COM_INIT_DB:
		con->client->packet_id++;
		network_mysqld_con_send_ok(con->client);
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
	network_socket *recv_sock;

	recv_sock = con->client;

	chunk = recv_sock->recv_queue->chunks->tail;
	s = chunk->data;

	if (s->len != recv_sock->packet_len + NET_HEADER_SIZE) return RET_SUCCESS;
	
	network_mysqld_con_handle_stmt(chas, con, s);
		
	con->parse.len = recv_sock->packet_len;

	g_string_free(chunk->data, TRUE);
	recv_sock->packet_len = PACKET_LEN_UNSET;

	g_queue_delete_link(recv_sock->recv_queue->chunks, chunk);

	con->state = CON_STATE_SEND_QUERY_RESULT;

	return RET_SUCCESS;
}

static int network_mysqld_server_connection_init(network_mysqld_con *con) {
	con->plugins.con_init             = server_con_init;

	con->plugins.con_read_auth        = server_read_auth;

	con->plugins.con_read_query       = server_read_query;

	return 0;
}

static chassis_plugin_config *network_mysqld_admin_plugin_init(void) {
	chassis_plugin_config *config;

	config = g_new0(chassis_plugin_config, 1);

	return config;
}

static void network_mysqld_admin_plugin_free(chassis_plugin_config *config) {
	if (config->listen_con) {
		/* the socket will be freed by network_mysqld_free() */
	}

	if (config->address) {
		g_free(config->address);
	}

	g_free(config);
}

/**
 * add the proxy specific options to the cmdline interface 
 */
static GOptionEntry * network_mysqld_admin_plugin_get_options(chassis_plugin_config *config) {
	guint i;

	static GOptionEntry config_entries[] = 
	{
		{ "admin-address",            0, 0, G_OPTION_ARG_STRING, NULL, "listening address:port of the admin-server (default: :4041)", "<host:port>" },
		
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	i = 0;
	config_entries[i++].arg_data = &(config->address);

	return config_entries;
}

/**
 * init the plugin with the parsed config
 */
static int network_mysqld_admin_plugin_apply_config(chassis *chas, chassis_plugin_config *config) {
	network_mysqld_con *con;
	network_socket *listen_sock;

	if (!config->address) config->address = g_strdup(":4041");

	/** 
	 * create a connection handle for the listen socket 
	 */
	con = network_mysqld_con_init();
	network_mysqld_add_connection(chas, con);
	con->config = config;

	config->listen_con = con;
	
	listen_sock = network_socket_init();
	con->server = listen_sock;

	/* set the plugin hooks as we want to apply them to the new connections too later */
	network_mysqld_server_connection_init(con);

	/* FIXME: network_socket_set_address() */
	if (0 != network_mysqld_con_set_address(&listen_sock->addr, config->address)) {
		return -1;
	}

	/* FIXME: network_socket_bind() */
	if (0 != network_mysqld_con_bind(listen_sock)) {
		return -1;
	}

	/**
	 * call network_mysqld_con_accept() with this connection when we are done
	 */
	event_set(&(listen_sock->event), listen_sock->fd, EV_READ|EV_PERSIST, network_mysqld_con_accept, con);
	event_base_set(chas->event_base, &(listen_sock->event));
	event_add(&(listen_sock->event), NULL);

	return 0;
}

G_MODULE_EXPORT int plugin_init(chassis_plugin *p) {
	p->magic        = CHASSIS_PLUGIN_MAGIC;
	p->name         = g_strdup("admin");

	p->init         = network_mysqld_admin_plugin_init;
	p->get_options  = network_mysqld_admin_plugin_get_options;
	p->apply_config = network_mysqld_admin_plugin_apply_config;
	p->destroy      = network_mysqld_admin_plugin_free;

	return 0;
}


