/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib.h>
#include <gmodule.h>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include "chassis-plugin.h"

chassis_plugin *chassis_plugin_init(void) {
	chassis_plugin *p;

	p = g_new0(chassis_plugin, 1);

	return p;
}

void chassis_plugin_free(chassis_plugin *p) {
	if (p->name) g_free(p->name);
	if (p->module) g_module_close(p->module);
	if (p->version) g_free(p->version);

	g_free(p);
}

chassis_plugin *chassis_plugin_load(const gchar *name) {
	int (*plugin_init)(chassis_plugin *p);
	chassis_plugin *p = chassis_plugin_init();

	p->module = g_module_open(name, G_MODULE_BIND_LOCAL);

	if (!p->module) {
		g_critical("loading module '%s' failed: %s", name, g_module_error());

		chassis_plugin_free(p);

		return NULL;
	}

	/* each module has to have a plugin_init function */
	if (!g_module_symbol(p->module, "plugin_init", (gpointer) &plugin_init)) {
		g_critical("module '%s' doesn't have a init-function: %s", name, g_module_error());
		chassis_plugin_free(p);
		return NULL;
	}

	if (0 != plugin_init(p)) {
		g_critical("init-function for module '%s' failed", name);
		chassis_plugin_free(p);
		return NULL;
	}

	if (p->magic != CHASSIS_PLUGIN_MAGIC) {
		g_critical("plugin '%s' doesn't match the current plugin interface (plugin is %lx, chassis is %lx)", name, p->magic, CHASSIS_PLUGIN_MAGIC);
		chassis_plugin_free(p);
		return NULL;
	}

	if (p->init) {
		p->config = p->init();
	}

	/* if the plugins haven't set p->name provide our own name */
	if (!p->name) p->name = g_strdup(name);
	/* set dummy version number if the plugin doesn't provide a real one */
	if (!p->version) {
		g_critical("plugin '%s' doesn't set a version number, refusing to load this plugin", name);
		chassis_plugin_free(p);
		return NULL;
	}

	return p;
}

GOptionEntry *chassis_plugin_get_options(chassis_plugin *p) {
	GOptionEntry * options;

	if (!p->get_options) return NULL;

	if (NULL == (options = p->get_options(p->config))) {
		g_critical("adding config options for module '%s' failed", p->name);
	}

	return options;
}


chassis_plugin* chassis_plugin_for_name(chassis *chas, gchar* plugin_name) {
    gsize i;

    if (!chas || !plugin_name) return NULL;

    /* search for a plugin named plugin_name */
	for (i = 0; i < chas->modules->len; i++) {
		chassis_plugin *p = chas->modules->pdata[i];
        if (!strcmp(p->name, plugin_name)) {
            return p;
        }
	}
    return NULL;
    
}

