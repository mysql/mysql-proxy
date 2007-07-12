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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>

#include "network-mysqld.h"
#include "network-mysqld-proto.h"
#include "sys-pedantic.h"

/**
 * signal handlers have to be volatile
 */
#ifdef _WIN32
volatile int agent_shutdown = 0;
#else
volatile sig_atomic_t agent_shutdown = 0;
#endif

static void signal_handler(int sig) {
	switch (sig) {
	case SIGINT: agent_shutdown = 1; break;
	case SIGTERM: agent_shutdown = 1; break;
	}
}

static void log_func(const gchar *UNUSED_PARAM(log_domain), GLogLevelFlags UNUSED_PARAM(log_level), const gchar *message, gpointer UNUSED_PARAM(user_data)) {
	write(STDERR_FILENO, message, strlen(message));
	write(STDERR_FILENO, "\n", 1);
}

void index_usage_rows(gpointer _key, gpointer _value, gpointer _rows) {
	GPtrArray *rows = _rows;
	GPtrArray *row;
	gchar *key = _key;
	network_mysqld_index_status *stats = _value;

	row = g_ptr_array_new();
	g_ptr_array_add(row, g_strdup(key));
	g_ptr_array_add(row, g_strdup_printf(F_U64, stats->used));
	g_ptr_array_add(row, g_strdup_printf("%u", stats->max_used_key_len));
	g_ptr_array_add(row, g_strdup_printf("%.4f", stats->avg_used_key_len));
	g_ptr_array_add(rows, row);
}

int index_usage_select(GPtrArray *fields, GPtrArray *rows, gpointer user_data) {
	MYSQL_FIELD *field;
	network_mysqld *srv = user_data;

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("name");
	field->org_name = g_strdup("name");
	field->type = FIELD_TYPE_STRING;
	field->flags = PRI_KEY_FLAG;
	field->length = 32;

	g_ptr_array_add(fields, field);

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("used");
	field->org_name = g_strdup("used");
	field->type = FIELD_TYPE_LONGLONG;
	field->flags = NUM_FLAG | UNSIGNED_FLAG;
	field->length = 11;
	g_ptr_array_add(fields, field);

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("max_used_key_len");
	field->org_name = g_strdup("max_used_key_len");
	field->type = FIELD_TYPE_LONGLONG;
	field->flags = NUM_FLAG | UNSIGNED_FLAG;
	field->length = 11;
	g_ptr_array_add(fields, field);

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("avg_used_key_len");
	field->org_name = g_strdup("avg_used_key_len");
	field->type = FIELD_TYPE_DOUBLE;
	field->flags = NUM_FLAG;
	field->length = 11;
	g_ptr_array_add(fields, field);

	g_hash_table_foreach(srv->index_usage, index_usage_rows, rows);

	return 0;
}

int config_select(GPtrArray *fields, GPtrArray *rows, gpointer user_data) {
	/**
	 * show the current configuration 
	 */
	network_mysqld *srv = user_data;
	MYSQL_FIELD *field;
	GPtrArray *row;
	gsize i;

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("option");
	field->org_name = g_strdup("option");
	field->type = FIELD_TYPE_STRING;
	field->flags = PRI_KEY_FLAG;
	field->length = 32;

	g_ptr_array_add(fields, field);

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("value");
	field->org_name = g_strdup("value");
	field->type = FIELD_TYPE_STRING;
	field->length = 32;

	g_ptr_array_add(fields, field);

#define RESULTSET_ADD(x) \
	row = g_ptr_array_new(); \
	g_ptr_array_add(row, g_strdup(#x)); \
	g_ptr_array_add(row, g_strdup_printf("%d", srv->config.x)); \
	g_ptr_array_add(rows, row);

#define RESULTSET_ADD_STR(x) \
	row = g_ptr_array_new(); \
	g_ptr_array_add(row, g_strdup(#x)); \
	g_ptr_array_add(row, g_strdup(srv->config.x)); \
	g_ptr_array_add(rows, row);

#define RESULTSET_ADD_STR_ARRAY(x) \
	for (i = 0; srv->config.x[i]; i++) { \
	row = g_ptr_array_new(); \
	g_ptr_array_add(row, g_strdup_printf("%s["F_SIZE_T"]", #x, i)); \
	g_ptr_array_add(row, g_strdup(srv->config.x[i])); \
	g_ptr_array_add(rows, row); \
	}

	RESULTSET_ADD_STR(admin.address);
	RESULTSET_ADD_STR(proxy.address);
	RESULTSET_ADD_STR(proxy.lua_script);
	RESULTSET_ADD_STR_ARRAY(proxy.backend_addresses);
	RESULTSET_ADD(proxy.fix_bug_25371);
	RESULTSET_ADD(proxy.profiling);

	return 0;
}

int connections_select(GPtrArray *fields, GPtrArray *rows, gpointer user_data) {
	network_mysqld *srv = user_data;
	/**
	 * show the current configuration 
	 *
	 * TODO: move to the proxy-module
	 */
	MYSQL_FIELD *field;
	gsize i;

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("id");
	field->type = FIELD_TYPE_LONG;
	field->flags = PRI_KEY_FLAG;
	field->length = 32;
	g_ptr_array_add(fields, field);

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("type");
	field->type = FIELD_TYPE_STRING;
	field->length = 32;
	g_ptr_array_add(fields, field);

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("state");
	field->type = FIELD_TYPE_STRING;
	field->length = 32;
	g_ptr_array_add(fields, field);

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("db");
	field->type = FIELD_TYPE_STRING;
	field->length = 64;
	g_ptr_array_add(fields, field);

	for (i = 0; i < srv->cons->len; i++) {
		GPtrArray *row;
		network_mysqld_con *rcon = srv->cons->pdata[i];

		if (!rcon) continue;
		if (rcon->is_listen_socket) continue;

		row = g_ptr_array_new();

		g_ptr_array_add(row, g_strdup_printf(F_SIZE_T, i));
		switch (rcon->config.network_type) {
		case NETWORK_TYPE_SERVER:
			g_ptr_array_add(row, g_strdup("server"));
			break;
		case NETWORK_TYPE_PROXY:
			g_ptr_array_add(row, g_strdup("proxy"));
			break;
		}

		g_ptr_array_add(row, g_strdup_printf("%d", rcon->state));
		g_ptr_array_add(row, g_strdup(rcon->default_db->len ? rcon->default_db->str : ""));
	
		g_ptr_array_add(rows, row);
	}

	return 0;
}

#ifndef _WIN32
/**
 * start the agent in the background 
 * 
 * UNIX-version
 */
static void daemonize(void) {
#ifdef SIGTTOU
	signal(SIGTTOU, SIG_IGN);
#endif
#ifdef SIGTTIN
	signal(SIGTTIN, SIG_IGN);
#endif
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
#endif
	if (fork() != 0) exit(0);
	
	if (setsid() == -1) exit(0);

	signal(SIGHUP, SIG_IGN);

	if (fork() != 0) exit(0);
	
	chdir("/");
	
	umask(0);
}
#endif


#define GETTEXT_PACKAGE "mysql-proxy"

int main(int argc, char **argv) {
	network_mysqld *srv;
	network_mysqld_table *table;
	
	/* read the command-line options */
	GOptionContext *option_ctx;
	GOptionGroup *option_grp;
	GError *gerr = NULL;
	int i;
	int exit_code = 0;
	int print_version = 0;
	int daemon_mode = 1;
	int start_proxy = 1;

	GOptionEntry admin_entries[] = 
	{
		{ "admin-address",            0, 0, G_OPTION_ARG_STRING, NULL, "listening address:port of pseudo mysql-server (default: :4041)", "<ip:port>" },
		
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	GOptionEntry proxy_entries[] = 
	{
		{ "proxy-address",            0, 0, G_OPTION_ARG_STRING, NULL, "listening address:port of the proxy-server (default: :4040)", "<ip:port>" },
		{ "proxy-read-only-address",  0, 0, G_OPTION_ARG_STRING, NULL, "listening address:port of the proxy-server for read-only connection (default: :4042)", "<ip:port>" },
		{ "proxy-backend-addresses",  0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, "address:port of the remote backend-servers (default: not set)", "<ip:port>" },
		
		{ "proxy-skip-profiling",     0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "disables profiling of queries (default: enabled)", NULL },

		{ "proxy-fix-bug-25371",      0, 0, G_OPTION_ARG_NONE, NULL, "fix bug #25371 (mysqld > 5.1.12) for older libmysql versions", NULL },
		{ "proxy-lua-script",         0, 0, G_OPTION_ARG_STRING, NULL, "filename of the lua script (default: not set)", "<file>" },
		
		{ "no-proxy",                 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "Don't start proxy-server", NULL },
		
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	GOptionEntry main_entries[] = 
	{
		{ "version",                 'V', 0, G_OPTION_ARG_NONE, NULL, "Show version", NULL },
		{ "no-daemon",               'D', G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "Don't start in daemon-mode", NULL },
		{ "pid-file",                 0, 0, G_OPTION_ARG_STRING, NULL, "PID file in case we are started as daemon", "<file>" },
		
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};


	srv = network_mysqld_init();
	srv->config.network_type          = NETWORK_TYPE_PROXY;  /* doesn't matter anymore */
	srv->config.proxy.fix_bug_25371   = 0; /** double ERR packet on AUTH failures */
	srv->config.proxy.profiling       = 1;

	i = 0;
	admin_entries[i++].arg_data = &(srv->config.admin.address);

	i = 0;
	proxy_entries[i++].arg_data = &(srv->config.proxy.address);
	proxy_entries[i++].arg_data = &(srv->config.proxy.read_only_address);
	proxy_entries[i++].arg_data = &(srv->config.proxy.backend_addresses);

	proxy_entries[i++].arg_data = &(srv->config.proxy.profiling);

	proxy_entries[i++].arg_data = &(srv->config.proxy.fix_bug_25371);
	proxy_entries[i++].arg_data = &(srv->config.proxy.lua_script);
	proxy_entries[i++].arg_data = &(start_proxy);

	i = 0;
	main_entries[i++].arg_data  = &(print_version);
	main_entries[i++].arg_data  = &(daemon_mode);
	main_entries[i++].arg_data  = &(srv->config.pid_file);

	g_log_set_default_handler(log_func, NULL);

	option_ctx = g_option_context_new("- MySQL Proxy");
	g_option_context_add_main_entries(option_ctx, main_entries, GETTEXT_PACKAGE);

	option_grp = g_option_group_new("admin", "admin module", "Show options for the admin-module", NULL, NULL);
	g_option_group_add_entries(option_grp, admin_entries);
	g_option_context_add_group(option_ctx, option_grp);

	option_grp = g_option_group_new("proxy", "proxy-module", "Show options for the proxy-module", NULL, NULL);
	g_option_group_add_entries(option_grp, proxy_entries);
	g_option_context_add_group(option_ctx, option_grp);
	
	if (FALSE == g_option_context_parse(option_ctx, &argc, &argv, &gerr)) {
		g_critical("%s", gerr->message);
		
		g_error_free(gerr);

		return -1;
	}

	g_option_context_free(option_ctx);

	if (print_version) {
		printf("%s\n", PACKAGE_STRING);
		return 0;
	}

	if (start_proxy) {
		if (!srv->config.proxy.address) srv->config.proxy.address = g_strdup(":4040");
		if (!srv->config.proxy.backend_addresses) {
			srv->config.proxy.backend_addresses = g_new0(char *, 2);
			srv->config.proxy.backend_addresses[0] = g_strdup("127.0.0.1:3306");
		}
	} else {
		if (srv->config.proxy.address) {
			g_free(srv->config.proxy.address);
			srv->config.proxy.address = NULL;
		}
	}

	if (!srv->config.admin.address) srv->config.admin.address = g_strdup(":4041");

	table = network_mysqld_table_init();
	table->select    = connections_select;
	table->user_data = srv;
	g_hash_table_insert(srv->tables, g_strdup("proxy_connections"), table);

	table = network_mysqld_table_init();
	table->select    = config_select;
	table->user_data = srv;
	g_hash_table_insert(srv->tables, g_strdup("proxy_config"), table);
	
	table = network_mysqld_table_init();
	table->select    = index_usage_select;
	table->user_data = srv;
	g_hash_table_insert(srv->tables, g_strdup("index_usage"), table);

#ifndef _WIN32	
	signal(SIGINT,  signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGPIPE, SIG_IGN);

	if (daemon_mode) {
		daemonize();
	}
#endif
	if (srv->config.pid_file) {
		int fd;
		gchar *pid_str;

		/**
		 * write the PID file
		 */

		if (-1 == (fd = open(srv->config.pid_file, O_WRONLY|O_TRUNC|O_CREAT, 0600))) {
			g_critical("%s.%d: open(%s) failed: %s", 
					__FILE__, __LINE__,
					srv->config.pid_file,
					strerror(errno));
			return -1;
		}

		pid_str = g_strdup_printf("%d", getpid());

		write(fd, pid_str, strlen(pid_str));
		g_free(pid_str);

		close(fd);
	}

	network_mysqld_init_libevent(srv);

	if (network_mysqld_thread(srv)) {
		/* looks like we failed */

		exit_code = -1;
	}

	network_mysqld_free(srv);

	return exit_code;
}

