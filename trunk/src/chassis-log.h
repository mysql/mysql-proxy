/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifndef _CHASSIS_LOG_H_
#define _CHASSIS_LOG_H_

#include <glib.h>

typedef struct {
	GLogLevelFlags min_lvl;

	gchar *log_filename;
	gint log_file_fd;

	gboolean use_syslog;

	gboolean rotate_logs;

	GString *log_ts_str;

	GString *last_msg;
	time_t   last_msg_ts;
	guint    last_msg_count;
} chassis_log;


chassis_log *chassis_log_init(void);
int chassis_log_set_level(chassis_log *log, const gchar *level);
void chassis_log_free(chassis_log *log);
int chassis_log_open(chassis_log *log);
void chassis_log_func(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data);

#endif
