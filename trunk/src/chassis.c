/* Copyright (C) 2007, 2008 MySQL AB */ 

/**
 * \mainpage
 *
 * \section Architecture
 *
 * MySQL Proxy is based around the C10k problem as described by http://kegel.com/c10k.html
 *
 * This leads to some basic features
 * - 10.000 concurrent connections in one program
 * - spreading the load over several backends
 * - each backend might be able to only handle 100 connections (max_connections)
 * 
 * We can implement 
 * - reusing idling backend connections 
 * - splitting client connections into several backend connections
 *
 * Most of the magic is happening in the scripting layer provided by lua (http://lua.org/) which
 * was picked as it:
 *
 * - is very easy to embed
 * - is small (200kb stripped) and efficient (see http://shootout.alioth.debian.org/gp4/benchmark.php?test=all&lang=all)
 * - is easy to read and write
 *
 * \section a walk through the code
 *
 * To understand the code you basicly only have to know about the three files documented below:
 *
 * - chassis.c
 *   - main()
 *     -# command-line handling
 *     -# plugin loading
 *     -# logging
 * - network-mysqld.c
 *   - network_mysqld_thread() (supposed be called as thread)
 *     -# registers event-halders (event_set(..., network_mysqld_con_accept, ...))
 *     -# calls event_base_dispatch() [libevent] in the mainloop 
 *   - network_mysqld_con_accept()
 *     -# is called when the listen()ing socket gets a incoming connection
 *     -# sets the event-handler for the established connection (e.g. network_mysqld_proxy_connection_init())
 *     -# calls network_mysqld_con_handle() on the connection 
 *   - network_mysqld_con_handle() is the state-machine
 *     -# implements the states of the \ref protocol "MySQL Protocol"
 *     -# calls plugin functions (registered by e.g. network_mysqld_proxy_connection_init()) 
 * - network-mysqld-proxy.c
 *   - implements the \ref proxy_states "proxy specific states"
 *
 * The other files only help those based main modules to do their job:
 *
 * - network-mysqld-proto.c
 *   - the byte functions around the \ref proto "MySQL protocol"
 * - network-socket.c
 *   - basic socket struct 
 * - network-mysqld-table.c
 *   - internal tables to select from on the admin interface (to be removed) 
 * - \ref sql-tokenizer.h "sql-tokenizer.y"
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
#include <gmodule.h>

#ifdef HAVE_LUA_H
#include <lua.h>
#endif

#include "network-mysqld.h"
#include "network-mysqld-proto.h"
#include "sys-pedantic.h"

#include "chassis-log.h"
#include "chassis-keyfile.h"
#include "chassis-mainloop.h"

/**
 * map SIGHUP to log->rotate_logs = true
 *
 * NOTE: signal handlers have to be volatile sig_atomic_t 
 */
#ifdef _WIN32
volatile int agent_rotate_logs = 0;
#else
volatile sig_atomic_t agent_rotate_logs = 0;
#endif

#ifdef _WIN32
static char **shell_argv;
static int shell_argc;
static int win32_running_as_service = 0;
static SERVICE_STATUS agent_service_status;
static SERVICE_STATUS_HANDLE agent_service_status_handle = 0;
#endif

#ifndef _WIN32
static void sighup_handler(int sig) {
	switch (sig) {
	case SIGHUP: agent_rotate_logs = 1; break;
	}
}
#endif

/**
 * turn a GTimeVal into string
 *
 * @return string in ISO notation with micro-seconds
 */
static gchar * g_timeval_string(GTimeVal *t1, GString *str) {
	size_t used_len;
	
	g_string_set_size(str, 63);

	used_len = strftime(str->str, str->allocated_len, "%Y-%m-%dT%H:%M:%S", gmtime(&t1->tv_sec));

	g_assert(used_len < str->allocated_len);
	str->len = used_len;

	g_string_append_printf(str, ".%06ld", t1->tv_usec);

	return str->str;
}



#ifndef _WIN32
/**
 * start the app in the background 
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

#ifdef _WIN32
/* win32 service */

void agent_service_set_state(DWORD new_state, int wait_msec) {
	DWORD status;
	
	g_assert(agent_service_status_handle);
	
	switch(new_state) {
		case SERVICE_START_PENDING:
		case SERVICE_STOP_PENDING:
			agent_service_status.dwWaitHint = wait_msec;
			
			if (agent_service_status.dwCurrentState == new_state) {
				agent_service_status.dwCheckPoint++;
			} else {
				agent_service_status.dwCheckPoint = 0;
			}
			
			break;
		default:
			agent_service_status.dwWaitHint = 0;
			break;
	}
	
	agent_service_status.dwCurrentState = new_state;
	
	if (!SetServiceStatus (agent_service_status_handle, &agent_service_status)) {
		status = GetLastError();
	}
}
#endif


/**
 * This is the "real" main which is called both on Windows and UNIX platforms.
 * For the Windows service case, this will also handle the notifications and set
 * up the logging support appropriately.
 */
int main_cmdline(int argc, char **argv) {
	chassis *srv;
	
	/* read the command-line options */
	GOptionContext *option_ctx;
	GError *gerr = NULL;
	guint i;
	int exit_code = EXIT_SUCCESS;
	int print_version = 0;
	int daemon_mode = 0;
	gchar *base_dir = NULL;
	const gchar *check_str = NULL;
	chassis_plugin *p;
	gchar *pid_file = NULL;
	gchar *plugin_dir = NULL;
	gchar *default_file = NULL;
	GOptionEntry *config_entries;
	gchar **plugin_names = NULL;

	gchar *log_level = NULL;

	GKeyFile *keyfile = NULL;
	chassis_log *log;

	/* holds argv[0] cleansed of the potential suffix on this platform (.exe on win32) */
	gchar *executable_name = NULL;
	
	/* can't appear in the configfile */
	GOptionEntry base_main_entries[] = 
	{
		{ "version",                 'V', 0, G_OPTION_ARG_NONE, NULL, "Show version", NULL },
		{ "defaults-file",            0, 0, G_OPTION_ARG_STRING, NULL, "configuration file", "<file>" },
		
		{ NULL,                       0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	GOptionEntry main_entries[] = 
	{
		{ "daemon",                   0, 0, G_OPTION_ARG_NONE, NULL, "Start in daemon-mode", NULL },
		{ "base-dir",                 0, 0, G_OPTION_ARG_STRING, NULL, "Base directory to prepend to relative paths in the config", "<absolute path>" },
		{ "pid-file",                 0, 0, G_OPTION_ARG_STRING, NULL, "PID file in case we are started as daemon", "<file>" },
		{ "plugin-dir",               0, 0, G_OPTION_ARG_STRING, NULL, "path to the plugins", "<path>" },
		{ "plugins",                  0, 0, G_OPTION_ARG_STRING_ARRAY, NULL, "plugins to load", "<name>" },
		{ "log-level",                0, 0, G_OPTION_ARG_STRING, NULL, "log all messages of level ... or higer", "(error|warning|info|message|debug)" },
		{ "log-file",                 0, 0, G_OPTION_ARG_STRING, NULL, "log all messages in a file", "<file>" },
		{ "log-use-syslog",           0, 0, G_OPTION_ARG_NONE, NULL, "log all messages to syslog", NULL },
		
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

	if (!g_module_supported()) {
		g_error("loading modules is not supported on this platform");
	}

#if defined(HAVE_LUA_H)
# if defined(DATADIR)
	/**
	 * if the LUA_PATH or LUA_CPATH are not set, set a good default 
	 */
	if (!g_getenv(LUA_PATH)) {
#if _WIN32
		/** on Win32 glib uses _wputenv to set the env variable,
		 *  but Lua uses getenv. Those two don't see each other,
		 *  so we use _putenv. Since we only set ASCII chars, this
		 *  is safe.
		 */
		_putenv(LUA_PATH "=!\\..\\" DATADIR "\\?.lua");
#else
		g_setenv(LUA_PATH, 
				DATADIR "/?.lua", 1);
#endif
	}

# endif

# if defined(LIBDIR)
	if (!g_getenv(LUA_CPATH)) {
#  if _WIN32
		g_setenv(LUA_CPATH, 
				LIBDIR "/?.dll", 1);
#  else
		g_setenv(LUA_CPATH, 
				LIBDIR "/?.so", 1);
#  endif
	}
# endif
#endif

#ifdef HAVE_GTHREAD	
	g_thread_init(NULL);
#endif

	log = chassis_log_init();
	log->min_lvl = G_LOG_LEVEL_MESSAGE; /* display messages while parsing or loading plugins */

#ifdef _WIN32
	if (win32_running_as_service) {
		log->use_windows_applog = TRUE;
		log->event_source_handle = RegisterEventSource(NULL, "mysql-monitor-agent");	/* TODO: get the actual executable name here */
		if (!log->event_source_handle) {
			int err = GetLastError();
			g_critical("unhandled error-code (%d) for RegisterEventSource(), shutting down", err);
			exit_code = EXIT_FAILURE;
			goto exit_nicely;
		}
	}
#endif
	g_log_set_default_handler(chassis_log_func, log);

	srv = chassis_init();
	/* assign the mysqld part to the */
	network_mysqld_init(srv);

	i = 0;
	base_main_entries[i++].arg_data  = &(print_version);
	base_main_entries[i++].arg_data  = &(default_file);

	i = 0;
	main_entries[i++].arg_data  = &(daemon_mode);
	main_entries[i++].arg_data  = &(base_dir);
	main_entries[i++].arg_data  = &(pid_file);
	main_entries[i++].arg_data  = &(plugin_dir);
	main_entries[i++].arg_data  = &(plugin_names);

	main_entries[i++].arg_data  = &(log_level);
	main_entries[i++].arg_data  = &(log->log_filename);
	main_entries[i++].arg_data  = &(log->use_syslog);

	option_ctx = g_option_context_new("- MySQL App Shell");
	g_option_context_add_main_entries(option_ctx, base_main_entries, GETTEXT_PACKAGE);
	g_option_context_set_help_enabled(option_ctx, FALSE);
	g_option_context_set_ignore_unknown_options(option_ctx, TRUE);

	/**
	 * parse once to get the basic options like --defaults-file and --version
	 *
	 * leave the unknown options in the list
	 */
	if (FALSE == g_option_context_parse(option_ctx, &argc, &argv, &gerr)) {
		g_critical("%s", gerr->message);
		
		exit_code = EXIT_FAILURE;
		goto exit_nicely;
	}

	if (default_file) {
		keyfile = g_key_file_new();
		g_key_file_set_list_separator(keyfile, ',');

		if (FALSE == g_key_file_load_from_file(keyfile, default_file, G_KEY_FILE_NONE, &gerr)) {
			g_critical("loading configuration from %s failed: %s", 
					default_file,
					gerr->message);

			exit_code = EXIT_FAILURE;
			goto exit_nicely;
		}
	}

	if (print_version) {
		printf("%s\r\n", PACKAGE_STRING); 
		printf("  glib2: %d.%d.%d\r\n", GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
#ifdef HAVE_EVENT_H
		printf("  libevent: %s\r\n", event_get_version());
#endif

		exit_code = EXIT_SUCCESS;
		goto exit_nicely;
	}


	/* add the other options which can also appear in the configfile */
	g_option_context_add_main_entries(option_ctx, main_entries, GETTEXT_PACKAGE);

	/**
	 * parse once to get the basic options 
	 *
	 * leave the unknown options in the list
	 */
	if (FALSE == g_option_context_parse(option_ctx, &argc, &argv, &gerr)) {
		g_critical("%s", gerr->message);

		exit_code = EXIT_FAILURE;
		goto exit_nicely;
	}

	if (keyfile) {
		if (chassis_keyfile_to_options(keyfile, "mysql-proxy", main_entries)) {
			exit_code = EXIT_FAILURE;
			goto exit_nicely;
		}
	}

	/* handle base_dir if set */
	if (base_dir) {
		/* base-dir must be an absolute path, doesn't make sense otherwise */
		if (!g_path_is_absolute(base_dir)) {
			/* TODO: here we have a problem, because our logging support is not yet set up.
			   What do we do on Windows when called as a service?
			 */
			g_critical("base-dir option must be an absolute path, but was %s", base_dir);
			exit_code = EXIT_FAILURE;
			goto exit_nicely;
		}
		
		/* copy it in case plugins need to use it */
		srv->base_dir = g_strdup(base_dir);

		/* fix our own global file names, if they are relative */
		chassis_resolve_path(srv, &log->log_filename);
		chassis_resolve_path(srv, &pid_file);
		chassis_resolve_path(srv, &plugin_dir);
	}

	if (log->log_filename) {
		if (0 == chassis_log_open(log)) {
			g_critical("can't open log-file '%s': %s", log->log_filename, g_strerror(errno));

			exit_code = EXIT_FAILURE;
			goto exit_nicely;
		}
	}


	/* handle log-level after the config-file is read, just in case it is specified in the file */
	if (log_level) {
		if (0 != chassis_log_set_level(log, log_level)) {
			g_critical("--log-level=... failed, level '%s' is unknown ", log_level);

			exit_code = EXIT_FAILURE;
			goto exit_nicely;
		}
	} else {
		/* if it is not set, use "critical" as default */
		log->min_lvl = G_LOG_LEVEL_CRITICAL;
	}

        /* Lets find the plugin directory relative the executable path */
        if (!plugin_dir) {
          gchar *absolute_path;
          gchar *bin_dir, *top_dir;
          if (g_path_is_absolute(argv[0])) {
            absolute_path = g_strdup(argv[0]);  /* No need to dup, just to get free right */
          } else {
            absolute_path = g_find_program_in_path(argv[0]);
            if (absolute_path == NULL)
              g_critical("can't find myself (%s) in PATH",argv[0]);
          }
          bin_dir = g_path_get_dirname(absolute_path);
          top_dir = g_path_get_dirname(bin_dir);
/* for Win32 the default plugin dir is bin/ and not lib/package_name */
#ifdef WIN32
          plugin_dir = g_strconcat(top_dir, G_DIR_SEPARATOR_S, "bin", NULL);
#else
          plugin_dir = g_strconcat(top_dir, G_DIR_SEPARATOR_S, "lib", G_DIR_SEPARATOR_S, PACKAGE, NULL);
#endif
          g_free(absolute_path);
          g_free(bin_dir);
          g_free(top_dir);
        }

	/* if not plugins are specified, load admin and proxy */
	if (!plugin_names) {
		plugin_names = g_new0(char *, 4); /* make sure we allocate _enough_ memory */

#define IS_PNAME(pname) \
		((strlen(executable_name) >= sizeof(pname) - 1) && \
		0 == strcmp(executable_name + strlen(executable_name) - (sizeof(pname) - 1), pname) \
		)

		/* on Windows allow for the executable suffix */
#ifdef WIN32
		if(g_str_has_suffix(argv[0], ".exe")) {
			executable_name = g_strndup(argv[0], strlen(argv[0]) - 4);	/* 4 is the length of ".exe" */
		} else {
			executable_name = g_strdup(argv[0]);
		}
#else
		executable_name = argv[0];
#endif
		/* check what we are called as */
		if (IS_PNAME("mysql-proxy")) {
			plugin_names[0] = g_strdup("admin");
			plugin_names[1] = g_strdup("proxy");
			plugin_names[2] = NULL;
		} else if (IS_PNAME("mysql-cli")) {
			plugin_names[0] = g_strdup("cli");
			plugin_names[1] = NULL;
		}
#ifdef WIN32
		/* cleanup executable_name since we dup'ed it */
		if (executable_name) {
			g_free(executable_name);
		}
#endif
	}

	/* load the plugins */
	for (i = 0; plugin_names && plugin_names[i]; i++) {
#ifdef WIN32
#define G_MODULE_PREFIX ""
#else
#define G_MODULE_PREFIX "lib"
#endif
/* we have to hack around some glib distributions that
 * don't set the correct G_MODULE_SUFFIX, notably MacPorts
 */
#ifndef SHARED_LIBRARY_SUFFIX
#define SHARED_LIBRARY_SUFFIX G_MODULE_SUFFIX
#endif
		char *plugin_filename = g_strdup_printf("%s%c%s%s.%s", 
				plugin_dir, 
				G_DIR_SEPARATOR, 
				G_MODULE_PREFIX,
				plugin_names[i],
				SHARED_LIBRARY_SUFFIX);
		
		p = chassis_plugin_load(plugin_filename);
		g_free(plugin_filename);
		
		if (NULL == p) {
			g_critical("setting --plugin-dir=<dir> might help");
			exit_code = EXIT_FAILURE;
			goto exit_nicely;
		}

		g_ptr_array_add(srv->modules, p);

		if (NULL != (config_entries = chassis_plugin_get_options(p))) {
			gchar *group_desc = g_strdup_printf("%s-module", plugin_names[i]);
			gchar *help_msg = g_strdup_printf("Show options for the %s-module", plugin_names[i]);
			const gchar *group_name = plugin_names[i];

			GOptionGroup *option_grp = g_option_group_new(group_name, group_desc, help_msg, NULL, NULL);
			g_option_group_add_entries(option_grp, config_entries);
			g_option_context_add_group(option_ctx, option_grp);

			g_free(help_msg);
			g_free(group_desc);

			/* parse the new options */
			if (FALSE == g_option_context_parse(option_ctx, &argc, &argv, &gerr)) {
				g_critical("%s", gerr->message);
		
				exit_code = EXIT_FAILURE;
				goto exit_nicely;
			}
	
			if (keyfile) {
				if (chassis_keyfile_to_options(keyfile, "mysql-proxy", config_entries)) {
					exit_code = EXIT_FAILURE;
					goto exit_nicely;
				}
			}
		}
	}

	/* we know about the options now, lets parse them */
	g_option_context_set_help_enabled(option_ctx, TRUE);
	g_option_context_set_ignore_unknown_options(option_ctx, FALSE);

	/* handle unknown options */
	if (FALSE == g_option_context_parse(option_ctx, &argc, &argv, &gerr)) {
		g_critical("%s", gerr->message);
		
		exit_code = EXIT_FAILURE;
		goto exit_nicely;
	}

	g_option_context_free(option_ctx);
	option_ctx = NULL;

	/* after parsing the options we should only have the program name left */
	if (argc > 1) {
		g_critical("unknown option: %s", argv[1]);

		exit_code = EXIT_FAILURE;
		goto exit_nicely;
	}


#ifndef _WIN32	
	signal(SIGPIPE, SIG_IGN);

	if (daemon_mode) {
		daemonize();
	}
#endif
	if (pid_file) {
		int fd;
		gchar *pid_str;

		/**
		 * write the PID file
		 */

		if (-1 == (fd = open(pid_file, O_WRONLY|O_TRUNC|O_CREAT, 0600))) {
			g_critical("%s.%d: open(%s) failed: %s", 
					__FILE__, __LINE__,
					pid_file,
					strerror(errno));

			exit_code = EXIT_FAILURE;
			goto exit_nicely;
		}

		pid_str = g_strdup_printf("%d", getpid());

		write(fd, pid_str, strlen(pid_str));
		g_free(pid_str);

		close(fd);
	}
#ifdef _WIN32
	agent_service_set_state(SERVICE_RUNNING, 0);
#endif

	if (chassis_mainloop(srv)) {
		/* looks like we failed */

		exit_code = EXIT_FAILURE;
		goto exit_nicely;
	}

exit_nicely:
	/* necessary to set the shutdown flag, because the monitor will continue
	 * to schedule timers otherwise, causing an infinite loop in cleanup
	 */
	chassis_set_shutdown();
#ifdef _WIN32
	if (win32_running_as_service) agent_service_set_state(SERVICE_STOP_PENDING, 0);
#endif
	
	if (keyfile) g_key_file_free(keyfile);
	if (default_file) g_free(default_file);

	if (gerr) g_error_free(gerr);
	if (option_ctx) g_option_context_free(option_ctx);
	if (srv) chassis_free(srv);

	if (base_dir) g_free(base_dir);
	if (pid_file) g_free(pid_file);
	if (log_level) g_free(log_level);
	if (plugin_dir) g_free(plugin_dir);

	if (plugin_names) {
		for (i = 0; plugin_names[i]; i++) {
			g_free(plugin_names[i]);
		}
		g_free(plugin_names);
	}

	chassis_log_free(log);
	
#ifdef _WIN32
	if (win32_running_as_service) agent_service_set_state(SERVICE_STOPPED, 0);
#endif
	
	return exit_code;
}

#ifdef _WIN32
/* win32 service */
/**
 the event-handler for the service
 
 the SCM will send us events from time to time which we acknoledge
 */

void WINAPI agent_service_ctrl(DWORD Opcode) {
	
	switch(Opcode) {
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			agent_service_set_state(SERVICE_STOP_PENDING, 0);
			
			chassis_set_shutdown(); /* exit the main-loop */
			
			break;
		default:
			agent_service_set_state(Opcode, 0);
			break;
	}
	
	return;
}

/**
 * trampoline us into the real main_cmdline
 */
void WINAPI agent_service_start(DWORD argc, LPTSTR *argv) {
	
	/* tell the service controller that we are alive */
	agent_service_status.dwCurrentState       = SERVICE_START_PENDING;
	agent_service_status.dwCheckPoint         = 0;
	agent_service_status.dwServiceType        = SERVICE_WIN32_OWN_PROCESS;
	agent_service_status.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	agent_service_status.dwWin32ExitCode      = NO_ERROR;
	agent_service_status.dwServiceSpecificExitCode = 0;
	
	agent_service_status_handle = RegisterServiceCtrlHandler("MerlinAgent", agent_service_ctrl); 
	
	if (agent_service_status_handle == (SERVICE_STATUS_HANDLE)0) {
		g_critical("RegisterServiceCtrlHandler failed");
		return; 
	}
	
	agent_service_set_state(SERVICE_START_PENDING, 1000);
	
	/* jump into the actual main */
	main_cmdline(shell_argc, shell_argv);
}

/**
 * Determine whether we are called as a service and set that up.
 * Then call main_cmdline to do the real work.
 */
int main_win32(int argc, char **argv) {
	WSADATA wsaData;

	SERVICE_TABLE_ENTRY dispatch_tab[] = {
		{ "MerlinAgent", agent_service_start },
		{ NULL, NULL } 
	};

	if (0 != WSAStartup(MAKEWORD( 1, 1 ), &wsaData)) {
		g_critical("WSAStartup failed to initialize the socket library.\n");

		return -1;
	}

	/* save the arguments because the service controller will clobber them */
	shell_argc = argc;
	shell_argv = argv;
	/* speculate that we are running as a service, reset to 0 on error */
	win32_running_as_service = 1;
	
	if (!StartServiceCtrlDispatcher(dispatch_tab)) {
		int err = GetLastError();
		
		switch(err) {
			case ERROR_FAILED_SERVICE_CONTROLLER_CONNECT:
				/* we weren't called as a service, carry on with the cmdline handling */
				win32_running_as_service = 0;
				return main_cmdline(shell_argc, shell_argv);
			case ERROR_SERVICE_ALREADY_RUNNING:
				g_critical("service is already running, shutting down");
				return 0;
			default:
				g_critical("unhandled error-code (%d) for StartServiceCtrlDispatcher(), shutting down", err);
				return -1;
		}
	}
	return 0;
}
#endif


/**
 * On Windows we first look if we are started as a service and 
 * set that up if appropriate.
 * We eventually fall down through to main_cmdline, even on Windows.
 */
int main(int argc, char **argv) {
#ifdef WIN32
	return main_win32(argc, argv);
#else
	return main_cmdline(argc, argv);
#endif
}
