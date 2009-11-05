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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef _WIN32
#include <stdio.h> /* for _getmaxstdio() */
#endif
#include <errno.h>

#include "chassis-limits.h"

/**
 * the size of rlim_t depends on arch and large-file-support
 */
#if SIZEOF_RLIM_T == 8
/* on MacOS X rlim_t is a __uint64_t which is a unsigned long long (which is a 64bit value)
 * GUINT64 is on 64bit a unsigned long ... well ... 
 *
 * even if they are the same size, gcc still spits out a warning ... we ignore it
 */
#define G_RLIM_T_FORMAT G_GUINT64_FORMAT
#else
#define G_RLIM_T_FORMAT G_GUINT32_FORMAT
#endif

int chassis_set_fdlimit(int max_files_number) {
#ifdef _WIN32
	g_debug("%s: current maximum number of open stdio file descriptors: %d", G_STRLOC, _getmaxstdio());
	if (max_files_number > 2048) {
		g_warning("%s: Windows only allows a maximum number of 2048 open files for stdio, using that value instead of %d", G_STRLOC, max_files_number);
		max_files_number = 2048;
	}
	if (-1 == _setmaxstdio(max_files_number)) {
		g_critical("%s: failed to increase the maximum number of open files for stdio: %s (%d)", G_STRLOC, g_strerror(errno), errno);
	} else {
		g_debug("%s: set new limit of open files for stdio to %d", G_STRLOC, _getmaxstdio());
	}
#else
	struct rlimit max_files_rlimit;

	if (-1 == getrlimit(RLIMIT_NOFILE, &max_files_rlimit)) {
		g_warning("%s: cannot get limit of open files for this process. %s (%d)",
				  G_STRLOC, g_strerror(errno), errno);
	} else {
		rlim_t soft_limit = max_files_rlimit.rlim_cur;
		g_debug("%s: current RLIMIT_NOFILE = %"G_RLIM_T_FORMAT" (hard: %"G_RLIM_T_FORMAT")", G_STRLOC, max_files_rlimit.rlim_cur, max_files_rlimit.rlim_max);

		max_files_rlimit.rlim_cur = max_files_number;

		g_debug("%s: trying to set new RLIMIT_NOFILE = %"G_RLIM_T_FORMAT" (hard: %"G_RLIM_T_FORMAT")", G_STRLOC, max_files_rlimit.rlim_cur, max_files_rlimit.rlim_max);
		if (-1 == setrlimit(RLIMIT_NOFILE, &max_files_rlimit)) {
			g_critical("%s: could not raise RLIMIT_NOFILE to %u, %s (%d). Current limit still %"G_RLIM_T_FORMAT".", G_STRLOC, max_files_number, g_strerror(errno), errno, soft_limit);
		} else {
			if (-1 == getrlimit(RLIMIT_NOFILE, &max_files_rlimit)) {
				g_warning("%s: cannot get limit of open files for this process. %s (%d)",
						  G_STRLOC, g_strerror(errno), errno);
			} else {
				g_debug("%s: set new RLIMIT_NOFILE = %"G_RLIM_T_FORMAT" (hard: %"G_RLIM_T_FORMAT")", G_STRLOC, max_files_rlimit.rlim_cur, max_files_rlimit.rlim_max);
			}
		}
	}
#endif
	return 0;
}

