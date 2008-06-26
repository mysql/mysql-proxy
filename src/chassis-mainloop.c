/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* event.h need struct timeval */
#endif

#include <glib.h>
#include "chassis-plugin.h"
#include "chassis-mainloop.h"
#include "chassis-log.h"

#ifdef _WIN32
static volatile int signal_shutdown;
#else
static volatile sig_atomic_t signal_shutdown;
#endif



/**
 * create a global context
 */
chassis *chassis_init() {
	chassis *chas;

	chas = g_new0(chassis, 1);

	chas->modules     = g_ptr_array_new();
	
	return chas;
}


/**
 * free the global scope
 *
 * closes all open connections, cleans up all plugins
 *
 * @param m      global context
 */
void chassis_free(chassis *chas) {
	guint i;
	const char *version;

	if (!chas) return;

	/* init the shutdown, without freeing share structures */	
	if (chas->priv_shutdown) chas->priv_shutdown(chas, chas->priv);

	/* call the destructor for all plugins */
	for (i = 0; i < chas->modules->len; i++) {
		chassis_plugin *p = chas->modules->pdata[i];

		g_assert(p->destroy);
		p->destroy(p->config);
		chassis_plugin_free(p);
	}
	
	g_ptr_array_free(chas->modules, TRUE);

	/* free the pointers _AFTER_ the modules are shutdown */
	if (chas->priv_free) chas->priv_free(chas, chas->priv);

#ifdef HAVE_EVENT_BASE_FREE
	/* only recent versions have this call */

	version = event_get_version();

	/* libevent < 1.3e doesn't cleanup its own fds from the event-queue in signal_init()
	 * calling event_base_free() would cause a assert() on shutdown
	 */
	if (version && (strcmp(version, "1.3e") >= 0)) {
		if (chas->event_base) event_base_free(chas->event_base);
	}
#endif
	
	if (chas->base_dir) g_free(chas->base_dir);
	g_free(chas);
}


void chassis_set_shutdown(void ) {
	signal_shutdown = 1;
}

gboolean chassis_is_shutdown() {
	return signal_shutdown == 1;
}

static void sigterm_handler(int G_GNUC_UNUSED fd, short G_GNUC_UNUSED event_type, void G_GNUC_UNUSED *_data) {
	chassis_set_shutdown();
}

static void sighup_handler(int G_GNUC_UNUSED fd, short G_GNUC_UNUSED event_type, void *_data) {
	chassis *chas = _data;

	g_message("received a SIGHUP, rotating logfile"); /* this should go into the old logfile */

	chassis_log_set_logrotate(chas->log);
	
	g_message("rotated logfile"); /* ... and this into the new one */
}


/**
 * Helper function to correctly take into account the users base-dir setting for
 * paths that might be relative.
 * Note: Because this function potentially frees the pointer to gchar* that's passed in and cannot lock
 *       on that, it is _not_ threadsafe. You have to ensure threadsafety yourself!
 * @returns TRUE if it modified the filename, FALSE if it didn't
 */
gboolean chassis_resolve_path(chassis *chas, gchar **filename) {
	gchar *new_path = NULL;

	/* nothing to do if we don't have a base_dir setting */
	g_assert(chas);
	if (!chas->base_dir ||
		!filename ||
		!*filename)
		return FALSE;
	
	/* don't even look at absolute paths */
	if (g_path_is_absolute(*filename)) return FALSE;
	
	new_path = g_build_filename(chas->base_dir, G_DIR_SEPARATOR_S, *filename, NULL);
	
	g_debug("%s.%d: adjusting relative path (%s) to base_dir (%s). New path: %s", __FILE__, __LINE__, *filename, chas->base_dir, new_path);

	g_free(*filename);
	*filename = new_path;
	return TRUE;
}

/**
 * init libevent
 *
 * kqueue has to be called after the fork() of daemonize
 *
 * @param m      global context
 */
static void chassis_init_libevent(chassis *chas) {
	chas->event_base  = event_init();
}

int chassis_mainloop(void *_chas) {
	chassis *chas = _chas;
	guint i;
	struct event ev_sigterm, ev_sigint, ev_sighup;

#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD( 2, 2 );

	err = WSAStartup( wVersionRequested, &wsaData );
	if ( err != 0 ) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		return err;	/* err is positive */
	}
#endif
	/* init the event-handlers */
	chassis_init_libevent(chas);

	/* setup all plugins all plugins */
	for (i = 0; i < chas->modules->len; i++) {
		chassis_plugin *p = chas->modules->pdata[i];

		g_assert(p->apply_config);
		if (0 != p->apply_config(chas, p->config)) {
			return -1;
		}
	}


	signal_set(&ev_sigterm, SIGTERM, sigterm_handler, NULL);
	event_base_set(chas->event_base, &ev_sigterm);
	signal_add(&ev_sigterm, NULL);

	signal_set(&ev_sigint, SIGINT, sigterm_handler, NULL);
	event_base_set(chas->event_base, &ev_sigint);
	signal_add(&ev_sigint, NULL);

#ifdef SIGHUP
	signal_set(&ev_sighup, SIGHUP, sighup_handler, chas);
	event_base_set(chas->event_base, &ev_sighup);
	if (signal_add(&ev_sighup, NULL)) {
		g_critical("%s: signal_add(SIGHUP) failed", G_STRLOC);
	}
#endif

	/**
	 * check once a second if we shall shutdown the proxy
	 */
	while (!chassis_is_shutdown()) {
		struct timeval timeout;
		int r;

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		g_assert(event_base_loopexit(chas->event_base, &timeout) == 0);

		r = event_base_dispatch(chas->event_base);

		if (r == -1) {
			if (errno == EINTR) continue;

			break;
		}
	}

	signal_del(&ev_sigterm);
	signal_del(&ev_sigint);
#ifdef SIGHUP
	signal_del(&ev_sighup);
#endif
	return 0;
}

