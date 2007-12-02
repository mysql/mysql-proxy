#ifndef _CHASSIS_KEYFILE_H_
#define _CHASSIS_KEYFILE_H_

#include <glib.h>

/**
 * parse the configfile options into option entries
 *
 */
int chassis_keyfile_to_options(GKeyFile *keyfile, const gchar *groupname, GOptionEntry *config_entries);

#endif

