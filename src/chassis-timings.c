/* $%BEGINLICENSE%$
 Copyright (C) 2009 Sun Microsystems, Inc

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

#include <glib.h>

#include "chassis-timings.h"

chassis_timestamps_global_t *chassis_timestamps_global = NULL;

chassis_timestamp_t *chassis_timestamp_new(void) {
	chassis_timestamp_t *ts;

	ts = g_new0(chassis_timestamp_t, 1);

	return ts;
}

void chassis_timestamp_init_now(chassis_timestamp_t *ts,
		const char *name,
		const char *filename,
		gint line) {

	ts->name = name;
	ts->filename = filename;
	ts->line = line;
	ts->usec = my_timer_microseconds();
	ts->cycles = my_timer_cycles();
	ts->ticks = my_timer_ticks();
}

void chassis_timestamp_free(chassis_timestamp_t *ts) {
	g_free(ts);
}

chassis_timestamps_t *chassis_timestamps_new(void) {
	chassis_timestamps_t *ts;

	ts = g_new0(chassis_timestamps_t, 1);
	ts->timestamps = NULL;

	return ts;

}

void chassis_timestamps_free(chassis_timestamps_t *ts) {
	g_list_free(ts->timestamps);
	g_free(ts);
}

void chassis_timestamps_add(chassis_timestamps_t *ts,
		const char *name,
		const char *filename,
		gint line) {
	chassis_timestamp_t *t;

	t = chassis_timestamp_new();
	chassis_timestamp_init_now(t, name, filename, line);

	ts->timestamps = g_list_append(ts->timestamps, t);
}

void chassis_timestamps_global_init(chassis_timestamps_global_t *gl) {
	chassis_timestamps_global_t *timestamps = gl;

	if (NULL == gl) {
		if (NULL != chassis_timestamps_global) {
			g_warning("%s: invalid attempt to reinitialize the global chassis timer info, ignoring call, still using %p",
					G_STRLOC, (void*)chassis_timestamps_global);
			return;
		} else {
			chassis_timestamps_global = g_new0(chassis_timestamps_global_t, 1);
		}
		timestamps = chassis_timestamps_global;
		g_debug("%s: created new global chassis timer info at %p", G_STRLOC, (void*)chassis_timestamps_global);
	}
	my_timer_init(timestamps);
}

void chassis_timestamps_global_free(chassis_timestamps_t *gl) {
	if (NULL == gl) {
		g_free(chassis_timestamps_global);
	} else {
		g_free(gl);
	}
}
