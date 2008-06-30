/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifndef _CHASSIS_PLUGIN_H_
#define _CHASSIS_PLUGIN_H_

#include <glib.h>
#include <gmodule.h>

#include "chassis-mainloop.h"
#include "chassis-exports.h"

/* current magic is 0.7.0-3 */
#define CHASSIS_PLUGIN_MAGIC 0x00070003L

typedef struct chassis_plugin_config chassis_plugin_config;

typedef struct chassis_plugin {
	long      magic;    /**< a magic token to verify that the plugin API matches */

	gchar    *name;     /**< the name of the plugin as defined */
	gchar    *version;  /**< the plugin's version number */
	GModule  *module;   /**< the plugin handle when loaded */
	chassis_plugin_config *config;  /**< contains the plugin-specific config data */

	chassis_plugin_config *(*init)(void);   /**< handler function to allocate/initialize a chassis_plugin_config struct */
	void     (*destroy)(chassis_plugin_config *user_data);  /**< handler function used to deallocate the chassis_plugin_config */
	GOptionEntry * (*get_options)(chassis_plugin_config *user_data); /**< handler function to obtain the command line argument information */
	int      (*apply_config)(chassis *chas, chassis_plugin_config * user_data); /**< handler function to set the argument values in the plugin's config */
    void*    (*get_global_state)(chassis_plugin_config *user_data, const char* member);     /**< handler function to retrieve the plugin's global state */
    
} chassis_plugin;

CHASSIS_API chassis_plugin *chassis_plugin_init(void);
CHASSIS_API chassis_plugin *chassis_plugin_load(const gchar *name);
CHASSIS_API void chassis_plugin_free(chassis_plugin *p);
CHASSIS_API GOptionEntry * chassis_plugin_get_options(chassis_plugin *p);

/**
 * Retrieve the chassis plugin for a particular name.
 * 
 * @param plugin_name The name of the plugin to look up.
 * @return A pointer to a chassis_plugin structure
 * @retval NULL if there is no loaded chassis with this name
 */
CHASSIS_API chassis_plugin* chassis_plugin_for_name(chassis *chas, gchar* plugin_name);

#endif
