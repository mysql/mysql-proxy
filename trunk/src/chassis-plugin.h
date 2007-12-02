#ifndef _CHASSIS_PLUGIN_H_
#define _CHASSIS_PLUGIN_H_

#include <glib.h>
#include <gmodule.h>

typedef struct chassis_plugin_config chassis_plugin_config;

typedef struct {
	gchar    *name;
	GModule  *module;
	chassis_plugin_config *config;

	chassis_plugin_config *(*init)(void);
	void     (*destroy)(chassis_plugin_config *user_data);
	GOptionEntry * (*get_options)(chassis_plugin_config *user_data);
	int      (*apply_config)(gpointer srv, chassis_plugin_config * user_data); /* network_mysqld, ... */
} chassis_plugin;

chassis_plugin *chassis_plugin_init(void);
chassis_plugin *chassis_plugin_load(const gchar *plugin_dir, const gchar *name);
void chassis_plugin_free(chassis_plugin *p);
GOptionEntry * chassis_plugin_get_options(chassis_plugin *p);

#endif
