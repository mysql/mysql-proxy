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

#include <check.h>

#include "agent-inst-config.h"

#define C(x) x, sizeof(x) - 1

/**
 * tests for the instance config parsing
 * @ingroup instconfig
 */


static void devnull_log_func(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data) {
	/* discard the output */
}


/*@{*/

/**
 * load 
 */
START_TEST(test_instconfig_load) {
	GLogFunc old_log_func;

	old_log_func = g_log_set_default_handler(devnull_log_func, NULL);

	g_log_set_default_handler(old_log_func, NULL);
} END_TEST
/*@}*/

Suite *instconfig_suite(void) {
	Suite *s = suite_create("plugin");
	TCase *tc_core = tcase_create("Core");

	suite_add_tcase (s, tc_core);
	tcase_add_test(tc_core, test_instconfig_load);

	return s;
}

int main() {
	int nf;
	Suite *s = instconfig_suite();
	SRunner *sr = srunner_create(s);
		
	srunner_run_all(sr, CK_ENV);

	nf = srunner_ntests_failed(sr);

	srunner_free(sr);
	
	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

