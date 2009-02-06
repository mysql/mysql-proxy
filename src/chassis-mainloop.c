/* $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ */
 

#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* event.h need struct timeval */
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>	 /* getpwnam() */
#endif

#include <glib.h>
#include "chassis-plugin.h"
#include "chassis-mainloop.h"
#include "chassis-event-thread.h"
#include "chassis-log.h"
#include "chassis-stats.h"

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
	
	chas->stats = chassis_stats_new();

	chas->threads = chassis_event_threads_new();

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
	if (chas->user) g_free(chas->user);
	
	if (chas->stats) chassis_stats_free(chas->stats);

	if (chas->threads) chassis_event_threads_free(chas->threads);

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
 * forward libevent messages to the glib error log 
 */
static void event_log_use_glib(int libevent_log_level, const char *msg) {
	/* map libevent to glib log-levels */

	GLogLevelFlags glib_log_level = G_LOG_LEVEL_DEBUG;

	if (libevent_log_level == _EVENT_LOG_DEBUG) glib_log_level = G_LOG_LEVEL_DEBUG;
	else if (libevent_log_level == _EVENT_LOG_MSG) glib_log_level = G_LOG_LEVEL_MESSAGE;
	else if (libevent_log_level == _EVENT_LOG_WARN) glib_log_level = G_LOG_LEVEL_WARNING;
	else if (libevent_log_level == _EVENT_LOG_ERR) glib_log_level = G_LOG_LEVEL_CRITICAL;

	g_log(G_LOG_DOMAIN, glib_log_level, "(libevent) %s", msg);
}


int chassis_mainloop(void *_chas) {
	chassis *chas = _chas;
	guint i;
	struct event ev_sigterm, ev_sigint, ev_sighup;
	chassis_event_thread_t *mainloop_thread;

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
	/* redirect logging from libevent to glib */
	event_set_log_callback(event_log_use_glib);


	/* add a event-handler for the "main" events */
	mainloop_thread = chassis_event_thread_new();
	chassis_event_threads_init_thread(chas->threads, mainloop_thread, chas);
	chassis_event_threads_add(chas->threads, mainloop_thread);

	chas->event_base = mainloop_thread->event_base; /* all global events go to the 1st thread */

	g_assert(chas->event_base);


	/* setup all plugins all plugins */
	for (i = 0; i < chas->modules->len; i++) {
		chassis_plugin *p = chas->modules->pdata[i];

		g_assert(p->apply_config);
		if (0 != p->apply_config(chas, p->config)) {
			return -1;
		}
	}

	/*
	 * drop root privileges if requested
	 */
#ifndef _WIN32
	if (chas->user) {
		struct passwd *user_info;
		uid_t user_id= geteuid();

		/* Don't bother if we aren't superuser */
		if (user_id) {
			g_critical("can only use the --user switch if running as root");
			return -1;
		}

		if (NULL == (user_info = getpwnam(chas->user))) {
			g_critical("unknown user: %s", chas->user);
			return -1;
		}

		if (chas->log->log_filename) {
			/* chown logfile */
			if (-1 == chown(chas->log->log_filename, user_info->pw_uid, user_info->pw_gid)) {
				g_critical("%s.%d: chown(%s) failed: %s",
							__FILE__, __LINE__,
							chas->log->log_filename,
							g_strerror(errno) );

				return -1;
			}
		}

		setgid(user_info->pw_gid);
		setuid(user_info->pw_uid);
		g_debug("now running as user: %s (%d/%d)",
				chas->user,
				user_info->pw_uid,
				user_info->pw_gid );
	}
#endif

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

	if (chas->event_thread_count < 1) chas->event_thread_count = 0;

	/* create the event-threads
	 *
	 * - dup the async-queue-ping-fds
	 * - setup the events notification
	 * */
	for (i = 0; i < chas->event_thread_count; i++) {
		chassis_event_thread_t *event_thread;
	
		event_thread = chassis_event_thread_new();
		chassis_event_threads_init_thread(chas->threads, event_thread, chas);
		chassis_event_threads_add(chas->threads, event_thread);
	}

	/* start the event threads */
	if (chas->event_thread_count > 0) {
		chassis_event_threads_start(chas->threads);
	}

	/**
	 * handle signals and all basic events into the main-thread
	 *
	 * block until we are asked to shutdown
	 */
	chassis_event_thread_loop(mainloop_thread);

	signal_del(&ev_sigterm);
	signal_del(&ev_sigint);
#ifdef SIGHUP
	signal_del(&ev_sighup);
#endif
	return 0;
}

