#ifndef _CAULDRON_PLUGIN_H_
#define _CAULDRON_PLUGIN_H_

#include <glib.h>
#include <gmodule.h>

typedef struct cauldron_plugin_config cauldron_plugin_config;

typedef struct {
	gchar    *name;
	GModule  *module;
	cauldron_plugin_config *config;

	cauldron_plugin_config *(*init)(void);
	void     (*destroy)(cauldron_plugin_config *user_data);
	GOptionEntry * (*get_options)(cauldron_plugin_config *user_data);
	int      (*apply_config)(gpointer srv, cauldron_plugin_config * user_data); /* network_mysqld, ... */
} cauldron_plugin;

cauldron_plugin *cauldron_plugin_init(void);
cauldron_plugin *cauldron_plugin_load(const gchar *plugin_dir, const gchar *name);
void cauldron_plugin_free(cauldron_plugin *p);
GOptionEntry * cauldron_plugin_get_options(cauldron_plugin *p);

#endif
