/* Copyright (C) 2007, 2008 MySQL AB
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "chassis-log.h"

#if GLIB_CHECK_VERSION(2, 16, 0)
#define C(x) x, sizeof(x) - 1

#define START_TEST(x) void (x)(void)

/**
 * Tests for the plugin interface
 * @ingroup plugin
 */

/*@{*/

/**
 * load 
 */
START_TEST(test_log_compress) {
	chassis_log *l;
	GLogFunc old_log_func;

	l = chassis_log_init();

	g_log_set_always_fatal(G_LOG_FATAL_MASK);

	old_log_func = g_log_set_default_handler(chassis_log_func, l);

	g_critical("I am duplicate");
	g_critical("I am duplicate");
	g_critical("I am duplicate");
	g_critical("above should be 'last message repeated 2 times'");

	g_log_set_default_handler(old_log_func, NULL);

	chassis_log_free(l);
}
/*@}*/

int main(int argc, char **argv) {
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/log_compress", test_log_compress);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
