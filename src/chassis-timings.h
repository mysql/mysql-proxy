#ifndef __CHASSIS_TIMINGS_H__
#define __CHASSIS_TIMINGS_H__

#include <glib.h>
#include "my_rdtsc.h"
#include "chassis-exports.h"

typedef struct {
	const gchar *name;
	guint64 usec;
	guint64 cycles;
	guint64 ticks;

	const gchar *filename;
	gint line;
} chassis_timestamp_t;

CHASSIS_API chassis_timestamp_t *chassis_timestamp_new(void);
void chassis_timestamp_init_now(chassis_timestamp_t *ts,
		const char *name,
		const char *filename,
		gint line);
CHASSIS_API void chassis_timestamp_free(chassis_timestamp_t *ts);

typedef struct {
	GList *timestamps; /* list of chassis_timestamp_t */
} chassis_timestamps_t;

CHASSIS_API chassis_timestamps_t *chassis_timestamps_new(void);
CHASSIS_API void chassis_timestamps_free(chassis_timestamps_t *ts);

CHASSIS_API void chassis_timestamps_add(chassis_timestamps_t *ts,
		const char *name,
		const char *filename,
		gint line);

typedef struct my_timer_info chassis_timestamps_global_t;
CHASSIS_API void chassis_timestamps_global_init(chassis_timestamps_global_t *gl);

#endif
