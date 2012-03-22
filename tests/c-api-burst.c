#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <mysql.h>

#include <glib.h>

#define C(x) (x), sizeof(x) - 1

int num_threads = 200;
int num_queries = 100;

GMutex *threads_connected_mutex = NULL;
GCond *threads_connected_cond = NULL;
int threads_connected = 0;

GMutex *queries_finished_mutex = NULL;
GCond *queries_finished_cond = NULL;
int queries_finished = 0;

GMutex *threads_running_mutex = NULL;
GCond *threads_running_cond = NULL;
int threads_running = 0;

typedef struct {
	gboolean sync_running;
	gboolean sync_connected;
	gboolean sync_queries;

	char *hostname;
	char *username;
	char *password;
	char *schemaname;
	char *socketpath;
	gint port;

	GString *query;

	guint id;
} connect_mysql_and_run_config;

connect_mysql_and_run_config *
connect_mysql_and_run_config_new(void) {
	connect_mysql_and_run_config *config;

	config = g_slice_new(connect_mysql_and_run_config);

	return config;
}

void
connect_mysql_and_run_config_free(connect_mysql_and_run_config *config) {
	g_slice_free(connect_mysql_and_run_config, config);
}

connect_mysql_and_run_config *
connect_mysql_and_run_config_dup(connect_mysql_and_run_config *src) {
	connect_mysql_and_run_config *dst;

	if (NULL == src) return NULL;

	dst = connect_mysql_and_run_config_new();
	dst->sync_running = src->sync_running;
	dst->sync_connected = src->sync_connected;
	dst->sync_queries = src->sync_queries;

	dst->hostname = g_strdup(src->hostname);
	dst->username = g_strdup(src->username);
	dst->password = g_strdup(src->password);
	dst->schemaname = g_strdup(src->schemaname);
	dst->socketpath = g_strdup(src->socketpath);

	dst->port = src->port;
	if (src->query) {
		dst->query = g_string_new_len(src->query->str, src->query->len);
	}

	return dst;
}

static connect_mysql_and_run_config *
connect_mysql_and_run(connect_mysql_and_run_config *config) {
	MYSQL      *mysql;
	int i;

	/* wait until all threads are up and running */
	g_mutex_lock(threads_running_mutex);
	threads_running++;
	if (config->sync_running) {
		if (threads_running == num_threads) {
			g_cond_broadcast(threads_running_cond);
		} else {
			g_cond_wait(threads_running_cond, threads_running_mutex);
		}
	}
	g_mutex_unlock(threads_running_mutex);

	/* init the mysql connection */
	mysql_thread_init();

	mysql = mysql_init(NULL);

	/* connect */
	if (NULL == mysql_real_connect(mysql, config->hostname, config->username, config->password, config->schemaname, config->port, config->socketpath, CLIENT_MULTI_RESULTS)) {
		/* we failed to connected, wake up the waiting threads in case we were the last not-connected thread */
		g_mutex_lock(threads_connected_mutex);
		threads_running--;
		if (config->sync_connected) {
			g_cond_broadcast(threads_connected_cond);
		}
		g_mutex_unlock(threads_connected_mutex);

		g_critical("%s: %s",
				G_STRLOC,
				mysql_error(mysql));
	
		mysql_close(mysql);

		mysql_thread_end();
		return config;
	}

	/* wait until all the other threads (that are still alive) are connected */
	g_mutex_lock(threads_connected_mutex);
	threads_connected++;
	if (threads_connected == threads_running) {
		if (config->sync_connected) {
			g_cond_broadcast(threads_connected_cond);
		}
		g_message("%s: %d threads are connected, starting %d rounds of queries (%s) in bursts",
				G_STRLOC,
				threads_connected,
				num_queries,
				config->query->str);
	} else {
		if (config->sync_connected) {
			while (threads_connected < threads_running) {
				g_cond_wait(threads_connected_cond, threads_connected_mutex);
			}
		}
	}
	g_mutex_unlock(threads_connected_mutex);

	for (i = 0; i < num_queries; i++) {
		MYSQL_RES *res;

		if (0 != mysql_real_query(mysql, config->query->str, config->query->len)) {
			g_critical("%s: [%d] executing query %s failed: %s", G_STRLOC,
					config->id,
					config->query->str,
					mysql_error(mysql));
			break;
		}

		/* throw away the resultset */
		if ((res = mysql_store_result(mysql))) {
			mysql_free_result(res);
		}

		/* wait until all other threads are finished */
		g_mutex_lock(queries_finished_mutex);
		queries_finished++;

		if (config->sync_queries) {
			if (queries_finished == threads_running) {
				queries_finished = 0;

				g_cond_broadcast(queries_finished_cond);
			} else {
				g_cond_wait(queries_finished_cond, queries_finished_mutex);
			}
		}
		g_mutex_unlock(queries_finished_mutex);
	}

	/* tear down connected and running */

	mysql_close(mysql);

	mysql_thread_end();

	return config;
}

int
main(int argc, char **argv) {
	GError *gerr = NULL;
	GThread **thrs;
	int i;
	GOptionGroup *opt_group;
	GOptionContext *opt_context;
	GOptionEntry opt_entries[] = {
		{ "threads", 0, 0, G_OPTION_ARG_INT, NULL, "number of threads", "<num>" },
		{ "queries", 0, 0, G_OPTION_ARG_INT, NULL, "number of queries to run per thread", "<num>" },
		{ "port", 0, 0, G_OPTION_ARG_INT, NULL, "port to connect to (default: 3306)", "<num>" },
		{ "hostname", 0, 0, G_OPTION_ARG_STRING, NULL, "hostname to connect to", "<name>" },
		{ "username", 0, 0, G_OPTION_ARG_STRING, NULL, "username (default: root)", "<name>" },
		{ "password", 0, 0, G_OPTION_ARG_STRING, NULL, "password (default: <empty>)", "<name>" },
		{ "schema", 0, 0, G_OPTION_ARG_STRING, NULL, "default schema", "<name>" },
		{ "socket", 0, 0, G_OPTION_ARG_FILENAME, NULL, "socket", "<path>" },
		{ "sync-running", 0, 0, G_OPTION_ARG_NONE, NULL, "sync before connect", NULL },
		{ "sync-connect", 0, 0, G_OPTION_ARG_NONE, NULL, "sync after connect", NULL },
		{ "sync-queries", 0, 0, G_OPTION_ARG_NONE, NULL, "sync after each query", NULL },
		{ "query", 0, 0, G_OPTION_ARG_STRING, NULL, "query to execute", "<string>" },

		{ NULL, 0, 0, G_OPTION_ARG_INT, NULL, "", "" }
	};
	connect_mysql_and_run_config config = {
		FALSE,
		FALSE,
		FALSE,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		3306,
		NULL
	};
	gchar *query = NULL;

	i = 0;
	opt_entries[i++].arg_data = &num_threads;
	opt_entries[i++].arg_data = &num_queries;
	opt_entries[i++].arg_data = &config.port;
	opt_entries[i++].arg_data = &config.hostname;
	opt_entries[i++].arg_data = &config.username;
	opt_entries[i++].arg_data = &config.password;
	opt_entries[i++].arg_data = &config.schemaname;
	opt_entries[i++].arg_data = &config.socketpath;
	opt_entries[i++].arg_data = &config.sync_running;
	opt_entries[i++].arg_data = &config.sync_connected;
	opt_entries[i++].arg_data = &config.sync_queries;
	opt_entries[i++].arg_data = &query;

	g_message("%s: started",
			G_STRLOC);

	opt_context = g_option_context_new("...");
	opt_group = g_option_group_new(
			"foo",
			"foo desc",
			"foo desc help",
			NULL,
			NULL);
	g_option_context_set_main_group(opt_context, opt_group);

	g_option_group_add_entries(opt_group, opt_entries);

	if (FALSE == g_option_context_parse(opt_context, &argc, &argv, &gerr)) {
		g_critical("%s: %s",
				G_STRLOC,
				gerr->message);

		g_clear_error(&gerr);

		return -1;
	}

	/* check if the sync-flags make sense */
	if (config.sync_queries && !config.sync_connected) {
		g_message("%s: --sync-queries needs --sync-connected, but --sync-connected wasn't set. Enabling it",
				G_STRLOC);

		config.sync_connected = TRUE;
	}

	if (config.sync_connected && !config.sync_running) {
		g_message("%s: --sync-connected needs --sync-running, but --sync-running wasn't set. Enabling it",
				G_STRLOC);

		config.sync_running = TRUE;
	}

	if (NULL == config.hostname) {
		config.hostname = g_strdup("127.0.0.1");
	}

	if (NULL == config.username) {
		config.username = g_strdup("root");
	}

	if (NULL == query) {
		config.query = g_string_new_len(C("SELECT 1"));
	} else {
		config.query = g_string_new(query);

		g_free(query);
		query = NULL;
	}

	/* init the libs, mutex and conditions */
	g_thread_init(NULL);
	mysql_library_init(argc, argv, NULL);

	threads_connected_cond = g_cond_new();
	threads_connected_mutex = g_mutex_new();

	threads_running_cond = g_cond_new();
	threads_running_mutex = g_mutex_new();

	queries_finished_cond = g_cond_new();
	queries_finished_mutex = g_mutex_new();

	/* ... and go */
	thrs = g_new0(GThread *, num_threads);
	for (i = 0; i < num_threads; i++) {
		connect_mysql_and_run_config *thr_conf = connect_mysql_and_run_config_dup(&config);

		thr_conf->id = i;

		if (NULL == (thrs[i] = g_thread_create((GThreadFunc)connect_mysql_and_run, thr_conf, TRUE, &gerr))) {
			g_critical("%s: creating thread failed: %s",
					G_STRLOC,
					gerr->message);
			g_clear_error(&gerr);
			return -1;
		}
	}

	g_message("%s: all %d threads created",
			G_STRLOC,
			num_threads);

	for (i = 0; i < num_threads; i++) {
		connect_mysql_and_run_config *thr_conf;
		GThread *thr = thrs[i];

		thr_conf = g_thread_join(thr);
		connect_mysql_and_run_config_free(thr_conf);
	}
	g_free(thrs);
	thrs = NULL;

	g_message("%s: all %d threads shutdown",
			G_STRLOC,
			num_threads);

	/* shutdown everything global */
	mysql_library_end();

	g_cond_free(threads_connected_cond);
	g_cond_free(threads_running_cond);
	g_cond_free(queries_finished_cond);

	g_mutex_free(threads_connected_mutex);
	g_mutex_free(threads_running_mutex);
	g_mutex_free(queries_finished_mutex);

	return 0;
}

