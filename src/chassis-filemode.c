/* $%BEGINLICENSE%$
 Copyright (C) 2009 MySQL AB, 2009 Sun Microsystems, Inc

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
 
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include <gmodule.h>

#include "chassis-filemode.h"

/*
 * check whether the given filename points to a file the permissions
 * of which are 0 for group and other (ie read/writable only by owner).
 * return 0 for "OK", -1 of the file cannot be accessed or is the wrong
 * type of file, and 1 if permissions are wrong
 *
 * since Windows has no concept of owner/group/other, this function
 * just return 0 for windows
 *
 * FIXME? this function currently ignores ACLs
 */
int
chassis_filemode_check(gchar *filename)
{
#ifndef _WIN32
	struct stat stbuf;
	mode_t		fmode;
	
	if (stat(filename, &stbuf) == -1) {
		g_critical("%s: cannot stat %s: %s", G_STRLOC, filename, 
				strerror(errno));
		return -1;
	}

	fmode = stbuf.st_mode;
	if ((fmode & S_IFMT) != S_IFREG) {
		g_critical("%s: %s is not a regular file", G_STRLOC, filename);
		return -1;
	}

#define MASK (S_IROTH|S_IWOTH|S_IXOTH)

	if ((fmode & MASK) != 0) {
		g_critical("%s: %s permissions not secure (0660 or stricter required)",
		    G_STRLOC, filename);
		return 1;
	}
	
#undef MASK

#endif /* _WIN32 */
	return 0;
}
