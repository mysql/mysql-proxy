#include <glib.h>

#include "chassis-timings.h"

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
	my_timer_init(gl);
}

