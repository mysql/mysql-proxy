/* Copyright (C) 2007, 2008 MySQL AB */ 

#ifndef _CHASSIS_KEYFILE_H_
#define _CHASSIS_KEYFILE_H_

#include <glib.h>

#include "chassis-exports.h"
/**
 * parse the configfile options into option entries
 *
 */
CHASSIS_API int chassis_keyfile_to_options(GKeyFile *keyfile, const gchar *groupname, GOptionEntry *config_entries);

#endif

