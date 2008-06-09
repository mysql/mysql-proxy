/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#ifndef WIN32
#include <unistd.h> /* close */
/* define eventlog types when not on windows, saves code below */
#define EVENTLOG_ERROR_TYPE	0x0001
#define EVENTLOG_WARNING_TYPE	0x0002
#define EVENTLOG_INFORMATION_TYPE	0x0004
#else
#include <windows.h>
#include <io.h>
#define STDERR_FILENO 2
#endif
#include <glib.h>

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#else
/* placeholder values for platforms not having syslog support */
#define LOG_CRIT	0
#define LOG_ERR	0
#define LOG_WARNING	0
#define LOG_NOTICE	0
#define LOG_INFO	0
#define LOG_DEBUG	0
#endif

#include "sys-pedantic.h"
#include "chassis-log.h"

#define S(x) x->str, x->len

/**
 * the mapping of our internal log levels various log systems
 */
const struct {
	char *name;
	GLogLevelFlags lvl;
	int syslog_lvl;
	int win_evtype;
} log_lvl_map[] = {	/* syslog levels are different to the glib ones */
	{ "error", G_LOG_LEVEL_ERROR,		LOG_CRIT,		EVENTLOG_ERROR_TYPE},
	{ "critical", G_LOG_LEVEL_CRITICAL, LOG_ERR,		EVENTLOG_ERROR_TYPE},
	{ "warning", G_LOG_LEVEL_WARNING,	LOG_WARNING,	EVENTLOG_WARNING_TYPE},
	{ "message", G_LOG_LEVEL_MESSAGE,	LOG_NOTICE,		EVENTLOG_INFORMATION_TYPE},
	{ "info", G_LOG_LEVEL_INFO,			LOG_INFO,		EVENTLOG_INFORMATION_TYPE},
	{ "debug", G_LOG_LEVEL_DEBUG,		LOG_DEBUG,		EVENTLOG_INFORMATION_TYPE},

	{ NULL, 0 }
};

chassis_log *chassis_log_init(void) {
	chassis_log *log;

	log = g_new0(chassis_log, 1);

	log->log_file_fd = -1;
	log->log_ts_str = g_string_sized_new(sizeof("2004-01-01T00:00:00.000Z"));
	log->min_lvl = G_LOG_LEVEL_CRITICAL;

	log->last_msg = g_string_new(NULL);
	log->last_msg_ts = 0;
	log->last_msg_count = 0;

	return log;
}

int chassis_log_set_level(chassis_log *log, const gchar *level) {
	gint i;

	for (i = 0; log_lvl_map[i].name; i++) {
		if (0 == strcmp(log_lvl_map[i].name, level)) {
			log->min_lvl = log_lvl_map[i].lvl;
			return 0;
		}
	}

	return -1;
}

int chassis_log_open(chassis_log *log) {
	log->log_file_fd = open(log->log_filename, O_RDWR | O_CREAT | O_APPEND, 0660);

	return (log->log_file_fd != -1);
}

int chassis_log_close(chassis_log *log) {
	if (log->log_file_fd == -1) return 0;

	close(log->log_file_fd);

	log->log_file_fd = -1;

	return 0;
}

void chassis_log_free(chassis_log *log) {
	if (!log) return;

	chassis_log_close(log);
#ifdef _WIN32
	if (log->event_source_handle) {
		if (!DeregisterEventSource(log->event_source_handle)) {
			int err = GetLastError();
			g_critical("unhandled error-code (%d) for DeregisterEventSource()", err);
		}
	}
#endif
	g_string_free(log->log_ts_str, TRUE);
	g_string_free(log->last_msg, TRUE);

	g_free(log);
}

static int chassis_log_update_timestamp(chassis_log *log) {
	struct tm *tm;
	time_t t;
	GString *s = log->log_ts_str;

	t = time(NULL);
	
	tm = localtime(&(t));
	
	s->len = strftime(s->str, s->allocated_len, "%Y-%m-%d %H:%M:%S", tm);
	
	return 0;
}

static int chassis_log_write(chassis_log *log, int log_level, GString *str) {
	if (-1 != log->log_file_fd) {
		/* prepend a timestamp */
		if (-1 == write(log->log_file_fd, S(str))) {
			/* writing to the file failed (Disk Full, what ever ... */
			
			write(STDERR_FILENO, S(str));
			write(STDERR_FILENO, "\n", 1);
		} else {
			write(log->log_file_fd, "\n", 1);
		}
#ifdef HAVE_SYSLOG_H
	} else if (log->use_syslog) {
		syslog(log_lvl_map[log_level].syslog_lvl, "%s", str->str);
#endif
#ifdef _WIN32
	} else if (log->use_windows_applog && log->event_source_handle) {
		char *log_messages[1];
		
		log_messages[0] = str->str;
		ReportEvent(log->event_source_handle,
					log_lvl_map[log_level].win_evtype,
					0, /* category, we don't have that yet */
					log_lvl_map[log_level].win_evtype, /* event indentifier, one of MSG_ERROR (0x01), MSG_WARNING(0x02), MSG_INFO(0x04) */
					NULL,
					1, /* number of strings to be substituted */
					0, /* no event specific data */
					log_messages,	/* the actual log message, always the message we came up with, we don't localize using Windows message files*/
					NULL);
#endif
	} else {
		write(STDERR_FILENO, S(str));
		write(STDERR_FILENO, "\n", 1);
	}

	return 0;
}

void chassis_log_func(const gchar *UNUSED_PARAM(log_domain), GLogLevelFlags log_level, const gchar *message, gpointer user_data) {
	chassis_log *log = user_data;
	int i;
	gchar *log_lvl_name = "(error)";
	gboolean is_duplicate = FALSE;

	/**
	 * make sure we syncronize the order of the write-statements 
	 */
	static GStaticMutex log_mutex = G_STATIC_MUTEX_INIT;

	/* ignore the verbose log-levels */
	if (log_level > log->min_lvl) return;

	g_static_mutex_lock(&log_mutex);

	for (i = 0; log_lvl_map[i].name; i++) {
		if (log_lvl_map[i].lvl == log_level) {
			log_lvl_name = log_lvl_map[i].name;
			break;
		}
	}

	if (log->last_msg->len > 0 &&
	    0 == strcmp(log->last_msg->str, message)) {
		is_duplicate = TRUE;
	}

	if (-1 != log->log_file_fd) {
		if (log->rotate_logs) {
			chassis_log_close(log);
			chassis_log_open(log);

			is_duplicate = FALSE; /* after a log-rotation always dump the queue */
		}
	}


	if (!is_duplicate ||
	    log->last_msg_count > 100 ||
	    time(NULL) - log->last_msg_ts > 30) {
		/* if we lave the last message repeating, log it */
		if (log->last_msg_count) {
			chassis_log_update_timestamp(log);
			g_string_append_printf(log->log_ts_str, ": (%s) last message repeated %d times",
					log_lvl_name,
					log->last_msg_count);

			chassis_log_write(log, log_level, log->log_ts_str);
		}
		chassis_log_update_timestamp(log);
		g_string_append(log->log_ts_str, ": (");
		g_string_append(log->log_ts_str, log_lvl_name);
		g_string_append(log->log_ts_str, ") ");
		g_string_append(log->log_ts_str, message);

		/* reset the last-logged message */	
		g_string_assign(log->last_msg, message);
		log->last_msg_count = 0;
		log->last_msg_ts = time(NULL);
			
		chassis_log_write(log, log_level, log->log_ts_str);
	} else {
		log->last_msg_count++;
	}

	log->rotate_logs = FALSE;

	g_static_mutex_unlock(&log_mutex);
}

void chassis_log_set_logrotate(chassis_log *log) {
	log->rotate_logs = TRUE;
}

