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

/** @addtogroup unittests Unit tests */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>

#include <glib.h>

#include "chassis-filemode.h"

#if GLIB_CHECK_VERSION(2, 16, 0)

#define TOO_OPEN	0666
#define GOOD_PERMS	0660
/**
 * @test 
 */
void test_file_permissions(void)
{
	char filename[MAXPATHLEN] = "/tmp/permsXXXXX";
	int	 fd;
	
	g_log_set_always_fatal(G_LOG_FATAL_MASK);

	/* 1st test: non-existent file */
	g_assert_cmpint(chassis_filemode_check("/tmp/a_non_existent_file"), ==, -1);

	fd = mkstemp(filename);

	/* 2nd test: too permissive */
	chmod(filename, TOO_OPEN);
	g_assert_cmpint(chassis_filemode_check(filename), ==, 1);

	/* 3rd test: OK */
	chmod(filename, GOOD_PERMS);
	g_assert_cmpint(chassis_filemode_check(filename), ==, 0);

	/* 4th test: non-regular file */
	close (fd);
	remove(filename);
	mkdir(filename, GOOD_PERMS);
	g_assert_cmpint(chassis_filemode_check(filename), ==, -1);

	/* clean up */
	rmdir(filename);
}
/*@}*/

int main(int argc, char **argv) {
	g_thread_init(NULL);

	
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");
	
	g_test_add_func("/core/basedir/fileperm", test_file_permissions);
	
	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
