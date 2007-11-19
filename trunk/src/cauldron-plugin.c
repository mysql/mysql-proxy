
#include <glib.h>
#include <gmodule.h>

#include "cauldron-plugin.h"

cauldron_plugin *cauldron_plugin_init(void) {
	cauldron_plugin *p;

	p = g_new0(cauldron_plugin, 1);

	return p;
}

void cauldron_plugin_free(cauldron_plugin *p) {
	if (p->name) g_free(p->name);
	if (p->module) g_module_close(p->module);

	g_free(p);
}

cauldron_plugin *cauldron_plugin_load(const gchar *moduledir, const gchar *name) {
	int (*plugin_init)(cauldron_plugin *p);
	cauldron_plugin *p = cauldron_plugin_init();
	gchar *path;

	p->name = g_strdup(name);

	path = g_module_build_path(moduledir, p->name);
	p->module = g_module_open(path, G_MODULE_BIND_LOCAL);

	if (!p->module) {
		g_critical("loading module '%s' from '%s' failed: %s", p->name, path, g_module_error());
		return NULL;
	}

	if (!g_module_symbol(p->module, "plugin_init", (gpointer) &plugin_init)) {
		g_critical("module '%s' doesn't have a init-function: %s", p->name, g_module_error());
		return NULL;
	}

	if (0 != plugin_init(p)) {
		g_critical("init-function for module '%s' failed", p->name);
		return NULL;
	}
	g_free(path);

	if (p->init) {
		p->config = p->init();
	}

	return p;
}

GOptionEntry *cauldron_plugin_get_options(cauldron_plugin *p) {
	GOptionEntry * options;

	if (!p->get_options) return NULL;

	if (NULL == (options = p->get_options(p->config))) {
		g_critical("adding config options for module '%s' failed", p->name);
	}

	return options;
}


