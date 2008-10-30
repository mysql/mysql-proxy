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

