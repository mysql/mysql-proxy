#ifndef _CHASSIS_PLUGIN_H_
#define _CHASSIS_PLUGIN_H_

#include <glib.h>
#include <gmodule.h>

#include "chassis-mainloop.h"

/* current magic is 0.7.0-1 */
#define CHASSIS_PLUGIN_MAGIC 0x00070001L

typedef struct chassis_plugin_config chassis_plugin_config;

typedef struct {
	long      magic; /* a magic token to verify that */

	gchar    *name;    /* the name of the plugin as defined */
	GModule  *module;  /* the plugin handle when loaded */
	chassis_plugin_config *config;

	chassis_plugin_config *(*init)(void);
	void     (*destroy)(chassis_plugin_config *user_data);
	GOptionEntry * (*get_options)(chassis_plugin_config *user_data);
	int      (*apply_config)(chassis *chas, chassis_plugin_config * user_data); /* chassis, ... */
} chassis_plugin;

chassis_plugin *chassis_plugin_init(void);
chassis_plugin *chassis_plugin_load(const gchar *plugin_dir, const gchar *name);
void chassis_plugin_free(chassis_plugin *p);
GOptionEntry * chassis_plugin_get_options(chassis_plugin *p);

#endif
