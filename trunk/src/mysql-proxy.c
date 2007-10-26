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

/**
 * \mainpage
 *
 * \section a walk through the code
 *
 * - mysql-proxy.c
 *   - main()
 *     -# command-line handling
 *     -# command-line handling
 *     -# command-line handling
 *   - internal SQL help for the admin interface (will be moved into a lua script)
 *     -# select * from proxy_connections
 *     -# select * from proxy_config
 *     -# select * from authors
 *     -# select * from help  
 * - network-mysqld.c
 *   - network_mysqld_thread() (supposed be called as thread)
 *     -# registers event-halders (event_set(..., network_mysqld_con_accept, ...))
 *     -# calls event_base_dispatch() [libevent] in the mainloop 
 *   - network_mysqld_con_accept()
 *     -# is called when the listen()ing socket gets a incoming connection
 *     -# sets the event-handler for the established connection (e.g. network_mysqld_proxy_connection_init())
 *     -# calls network_mysqld_con_handle() on the connection 
 *   - network_mysqld_con_handle() is the state-machine
 *     -# \image html http://forge.mysql.com/w/images/6/6e/Mysql-proto-state.png
 *     -# implements the states of the MySQL Protocol
 *       -# connect, handshake, old-password, query, result 
 *     -# calls plugin functions (registered by e.g. network_mysqld_proxy_connection_init()) 
 *
 * - network-mysqld-proxy.c
 *   - network_mysqld_proxy_connection_init()
 *     -# registers the callbacks 
 *   - proxy_connect_server() (CON_STATE_CONNECT_SERVER)
 *     -# calls the connect_server() function in the lua script which might decide to
 *       -# send a handshake packet without contacting the backend server (CON_STATE_SEND_HANDSHAKE)
 *       -# closing the connection (CON_STATE_ERROR)
 *       -# picking a active connection from the connection pool
 *       -# pick a backend to authenticate against
 *       -# do nothing 
 *     -# by default, pick a backend from the backend list on the backend with the least active connctions
 *     -# opens the connection to the backend with connect()
 *     -# when done CON_STATE_READ_HANDSHAKE 
 *   - proxy_read_handshake() (CON_STATE_READ_HANDSHAKE)
 *     -# reads the handshake packet from the server 
 *   - proxy_read_auth() (CON_STATE_READ_AUTH)
 *     -# reads the auth packet from the client 
 *   - proxy_read_auth_result() (CON_STATE_READ_AUTH_RESULT)
 *     -# reads the auth-result packet from the server 
 *   - proxy_send_auth_result() (CON_STATE_SEND_AUTH_RESULT)
 *   - proxy_read_query() (CON_STATE_READ_QUERY)
 *     -# reads the query from the client 
 *   - proxy_read_query_result() (CON_STATE_READ_QUERY_RESULT)
 *     -# reads the query-result from the server 
 *   - proxy_send_query_result() (CON_STATE_SEND_QUERY_RESULT)
 *     -# called after the data is written to the client
 *     -# if scripts wants to close connections, goes to CON_STATE_ERROR
 *     -# if queries are in the injection queue, goes to CON_STATE_SEND_QUERY
 *     -# otherwise goes to CON_STATE_READ_QUERY
 *     -# does special handling for COM_BINLOG_DUMP (go to CON_STATE_READ_QUERY_RESULT) 
 *
 * The other files only help those based main modules to do their job:
 *
 * - network-mysqld-proto.c
 *   - the byte functions around the MySQL protocol 
 * - network-socket.c
 *   - basic socket struct 
 * - network-mysqld-table.c
 *   - internal tables to select from on the admin interface (to be removed) 
 * - sql-tokenizer.y
 *   - a flex based tokenizer for MySQL's SQL dialect (no parser) 
 * - network-conn-pool.c
 *   - a connection pool for server connections 
 */


/** @file
 * the user-interface for the MySQL Proxy @see main()
 *
 *  -  command-line handling 
 *  -  config-file parsing
 * 
 *
 * network_mysqld_thread() is the real proxy thread 
 * 
 * @todo move the SQL based help out into a lua script
 */


#define SVN_REVISION "$Rev$"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#ifdef _WIN32
#include <process.h> /* getpid() */
#include <io.h>      /* open() */
#else
#include <unistd.h>
#endif

#include <glib.h>

#ifdef HAVE_LUA_H
#include <lua.h>
#endif

#include "network-mysqld.h"
#include "network-mysqld-proto.h"
#include "sys-pedantic.h"

/**
 * signal handlers have to be volatile
 */
#ifdef _WIN32
volatile int agent_shutdown = 0;
#define STDERR_FILENO 2
#else
volatile sig_atomic_t agent_shutdown = 0;
#endif

#ifndef _WIN32
static void signal_handler(int sig) {
	switch (sig) {
	case SIGINT: agent_shutdown = 1; break;
	case SIGTERM: agent_shutdown = 1; break;
	}
}
#endif

/**
 * log everything to the stdout for now
 */
static void log_func(const gchar *UNUSED_PARAM(log_domain), GLogLevelFlags UNUSED_PARAM(log_level), const gchar *message, gpointer UNUSED_PARAM(user_data)) {
	write(STDERR_FILENO, message, strlen(message));
	write(STDERR_FILENO, "\n", 1);
}

struct authors_st {
	const char *name;
	const char *location;
	const char *role;
};

/**
 * list of contributors 
 */
static struct authors_st table_authors[]= {
	{"Jan Kneschke", 	"Kiel, Germany", 	"Design, development, applications, articles"},
	{"Giuseppe Maxia", 	"Cagliari, Italy", 	"Testing, applications, articles"},
	{"Lenz Grimmer", 	"Hamburg, Germany", "RPM packaging, openSUSE packages"},
	{"Martin MC Brown", "Grantham, UK",     "Documentation"},
    /* Add new authors here */
    NULL
};

int authors_select(GPtrArray *fields, GPtrArray *rows, gpointer user_data) {
	/**
	 * show the authors
	 */
	network_mysqld *srv = user_data;
	MYSQL_FIELD *field;
	GPtrArray *row;
    int counter;
    char *field_descr[] = {
			"name", 
			"location", 
			"role", 
			NULL
	};
   
    for (counter = 0; field_descr[counter] != NULL ; counter++) {
    	field = network_mysqld_proto_field_init();
	    field->name = g_strdup(field_descr[counter]);
	    field->org_name = g_strdup(field_descr[counter]);
	    field->type = FIELD_TYPE_STRING;
	    field->flags = PRI_KEY_FLAG;
	    field->length = 60;
	    g_ptr_array_add(fields, field);
    } 

	for (counter = 0; table_authors[counter].name != NULL ; counter++) {
		row = g_ptr_array_new(); 
		g_ptr_array_add(row, g_strdup(table_authors[counter].name)); 
		g_ptr_array_add(row, g_strdup(table_authors[counter].location)); 
		g_ptr_array_add(row, g_strdup(table_authors[counter].role)); 
		g_ptr_array_add(rows, row);
	}
	return 0;
}

int help_select(GPtrArray *fields, GPtrArray *rows, gpointer user_data) {
	/**
	 * show the available commands 
	 */
	network_mysqld *srv = user_data;
	MYSQL_FIELD *field;
	GPtrArray *row;

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("command");
	field->org_name = g_strdup("command");
	field->type = FIELD_TYPE_STRING;
	field->flags = PRI_KEY_FLAG;
	field->length = 50;

	g_ptr_array_add(fields, field);

	field = network_mysqld_proto_field_init();
	field->name = g_strdup("description");
	field->org_name = g_strdup("description");
	field->type = FIELD_TYPE_STRING;
	field->length = 80;

	g_ptr_array_add(fields, field);

	row = g_ptr_array_new(); 
	g_ptr_array_add(row, g_strdup("select * from proxy_connections")); 
	g_ptr_array_add(row, g_strdup("show information about proxy connections")); 
	g_ptr_array_add(rows, row);

	row = g_ptr_array_new(); 
	g_ptr_array_add(row, g_strdup("select * from proxy_config")); 
	g_ptr_array_add(row, g_strdup("show information about proxy configuration")); 
	g_ptr_array_add(rows, row);

	row = g_ptr_array_new(); 
	g_ptr_array_add(row, g_strdup("select * from authors")); 
	g_ptr_array_add(row, g_strdup("show information about the authors")); 
	g_ptr_array_add(rows, row);


    /*
     * Add new command descriptions above this comment
     *
     * */

	row = g_ptr_array_new(); 
	g_ptr_array_add(row, g_strdup("select * from help")); 
	g_ptr_array_add(row, g_strdup("show the available commands")); 
	g_ptr_array_add(rows, row);

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

/**
 * show the current configuration 
 *
 * @todo move to the proxy-module
 */
int connections_select(GPtrArray *fields, GPtrArray *rows, gpointer user_data) {
	network_mysqld *srv = user_data;
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
		g_ptr_array_add(row, g_strdup(rcon && rcon->server && rcon->server->default_db->len ? rcon->server->default_db->str : ""));
	
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
	int daemon_mode = 0;
	int start_proxy = 1;
	const gchar *check_str = NULL;

	GOptionEntry admin_entries[] = 
	{
		{ "admin-address",            0, 0, G_OPTION_ARG_STRING, NULL, "listening address:port of internal admin-server (default: :4041)", "<host:port>" },
		
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	GOptionEntry proxy_entries[] = 
	{
		{ "proxy-address",            0, 0, G_OPTION_ARG_STRING, NULL, "listening address:port of the proxy-server (default: :4040)", "<host:port>" },
		{ "proxy-read-only-backend-addresses", 
					      0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, "address:port of the remote slave-server (default: not set)", "<host:port>" },
		{ "proxy-backend-addresses",  0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, "address:port of the remote backend-servers (default: 127.0.0.1:3306)", "<host:port>" },
		
		{ "proxy-skip-profiling",     0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "disables profiling of queries (default: enabled)", NULL },

		{ "proxy-fix-bug-25371",      0, 0, G_OPTION_ARG_NONE, NULL, "fix bug #25371 (mysqld > 5.1.12) for older libmysql versions", NULL },
		{ "proxy-lua-script",         0, 0, G_OPTION_ARG_STRING, NULL, "filename of the lua script (default: not set)", "<file>" },
		
		{ "no-proxy",                 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, NULL, "Don't start proxy-server", NULL },
		
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	GOptionEntry main_entries[] = 
	{
		{ "version",                 'V', 0, G_OPTION_ARG_NONE, NULL, "Show version", NULL },
		{ "daemon",                   0, 0, G_OPTION_ARG_NONE, NULL, "Start in daemon-mode", NULL },
		{ "pid-file",                 0, 0, G_OPTION_ARG_STRING, NULL, "PID file in case we are started as daemon", "<file>" },
		
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	if (!GLIB_CHECK_VERSION(2, 6, 0)) {
		g_error("the glib header are too old, need at least 2.6.0, got: %d.%d.%d", 
				GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
	}

	check_str = glib_check_version(GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);

	if (check_str) {
		g_error("%s, got: lib=%d.%d.%d, headers=%d.%d.%d", 
			check_str,
			glib_major_version, glib_minor_version, glib_micro_version,
			GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
	}
	
	srv = network_mysqld_init();
	srv->config.network_type          = NETWORK_TYPE_PROXY;  /* doesn't matter anymore */
	srv->config.proxy.fix_bug_25371   = 0; /** double ERR packet on AUTH failures */
	srv->config.proxy.profiling       = 1;

	i = 0;
	admin_entries[i++].arg_data = &(srv->config.admin.address);

	i = 0;
	proxy_entries[i++].arg_data = &(srv->config.proxy.address);
	proxy_entries[i++].arg_data = &(srv->config.proxy.read_only_backend_addresses);
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

#if defined(HAVE_LUA_H) && defined(LIBDIR)
	/**
	 * if the LUA_PATH is not set, set a good default 
	 */
	if (!g_getenv("LUA_PATH")) {
		g_setenv("LUA_PATH", LUA_PATHSEP LUA_PATHSEP LIBDIR "/?.lua", 1);
	}
#endif

	if (print_version) {
		printf("%s\r\n", PACKAGE_STRING);
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

    /*
     *  If you add a new command, please update help_select() above
     *
     */
	table = network_mysqld_table_init();
	table->select    = connections_select;
	table->user_data = srv;
	g_hash_table_insert(srv->tables, g_strdup("proxy_connections"), table);

	table = network_mysqld_table_init();
	table->select    = config_select;
	table->user_data = srv;
	g_hash_table_insert(srv->tables, g_strdup("proxy_config"), table);
	
	table = network_mysqld_table_init();
	table->select    = help_select;
	table->user_data = srv;
	g_hash_table_insert(srv->tables, g_strdup("help"), table);

	table = network_mysqld_table_init();
	table->select    = authors_select;
	table->user_data = srv;
	g_hash_table_insert(srv->tables, g_strdup("authors"), table);


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

