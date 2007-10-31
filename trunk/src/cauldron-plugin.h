#ifndef _CAULDRON_PLUGIN_H_
#define _CAULDRON_PLUGIN_H_

#include <glib.h>
#include <gmodule.h>

typedef struct cauldron_plugin_config cauldron_plugin_config;

typedef struct {
	gchar    *name;
	GModule  *module;
	gpointer config;

	cauldron_plugin_config *(*init)(void);
	void     (*destroy)(cauldron_plugin_config *user_data);
	int      (*add_options)(GOptionContext *option_ctx, cauldron_plugin_config *user_data);
	int      (*apply_config)(gpointer srv, cauldron_plugin_config * user_data); /* network_mysqld, ... */
} cauldron_plugin;

cauldron_plugin *cauldron_plugin_init(void);
cauldron_plugin *cauldron_plugin_load(const gchar *name);
void cauldron_plugin_free(cauldron_plugin *p);
int cauldron_plugin_add_options(cauldron_plugin *p, GOptionContext *option_ctx);

#endif
