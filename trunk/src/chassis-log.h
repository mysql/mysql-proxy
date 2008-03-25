/* Copyright (C) 2007, 2008 MySQL AB
 
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
