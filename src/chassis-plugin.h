/* $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

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
 

#ifndef _CHASSIS_PLUGIN_H_
#define _CHASSIS_PLUGIN_H_

#include <glib.h>
#include <gmodule.h>

#include "chassis-mainloop.h"
#include "chassis-exports.h"

/* current magic is 0.7.0-4 */
#define CHASSIS_PLUGIN_MAGIC 0x00070004L

/**
 * The private stats structure of a plugin. This is opaque to the rest of the code,
 * we can only get a copy of it in a hash.
 * @see chassis_plugin_stats.get_stats()
 */
typedef struct chassis_plugin_stats chassis_plugin_stats_t;
typedef struct chassis_plugin_config chassis_plugin_config;

typedef struct chassis_plugin {
	long      magic;    /**< a magic token to verify that the plugin API matches */

	gchar    *name;     /**< the name of the plugin as defined */
	gchar    *version;  /**< the plugin's version number */
	GModule  *module;   /**< the plugin handle when loaded */
	
	chassis_plugin_stats_t *stats;	/**< contains the plugin-specific statistics */

	chassis_plugin_stats_t *(*new_stats)(void);		/**< handler function to initialize the plugin-specific stats */
	void (*free_stats)(chassis_plugin_stats_t *user_data);	/**< handler function to dealloc the plugin-specific stats */
	GHashTable *(*get_stats)(chassis_plugin_stats_t *user_data);	/**< handler function to retrieve the plugin-specific stats */

	chassis_plugin_config *config;  /**< contains the plugin-specific config data */

	chassis_plugin_config *(*init)(void);   /**< handler function to allocate/initialize a chassis_plugin_config struct */
	void     (*destroy)(chassis_plugin_config *user_data);  /**< handler function used to deallocate the chassis_plugin_config */
	GOptionEntry * (*get_options)(chassis_plugin_config *user_data); /**< handler function to obtain the command line argument information */
	int      (*apply_config)(chassis *chas, chassis_plugin_config * user_data); /**< handler function to set the argument values in the plugin's config */
    void*    (*get_global_state)(chassis_plugin_config *user_data, const char* member);     /**< handler function to retrieve the plugin's global state */
    
} chassis_plugin;

CHASSIS_API chassis_plugin *chassis_plugin_init(void) G_GNUC_DEPRECATED;
CHASSIS_API chassis_plugin *chassis_plugin_new(void);
CHASSIS_API chassis_plugin *chassis_plugin_load(const gchar *name);
CHASSIS_API void chassis_plugin_free(chassis_plugin *p);
CHASSIS_API GOptionEntry * chassis_plugin_get_options(chassis_plugin *p);

/**
 * Retrieve the chassis plugin for a particular name.
 * 
 * @param chas        a pointer to the chassis
 * @param plugin_name The name of the plugin to look up.
 * @return A pointer to a chassis_plugin structure
 * @retval NULL if there is no loaded chassis with this name
 */
CHASSIS_API chassis_plugin* chassis_plugin_for_name(chassis *chas, gchar* plugin_name);

#endif
