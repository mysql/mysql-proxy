#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <check.h>

#include "chassis-log.h"

#define C(x) x, sizeof(x) - 1

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

	old_log_func = g_log_set_default_handler(chassis_log_func, l);

	g_critical("I am duplicate");
	g_critical("I am duplicate");
	g_critical("I am duplicate");
	g_critical("above should be 'last message repeated 2 times'");

	g_log_set_default_handler(old_log_func, NULL);

	chassis_log_free(l);
} END_TEST
/*@}*/

Suite *plugin_suite(void) {
	Suite *s = suite_create("plugin");
	TCase *tc_core = tcase_create("Core");

	suite_add_tcase (s, tc_core);
	tcase_add_test(tc_core, test_log_compress);

	return s;
}

int main() {
	int nf;
	Suite *s = plugin_suite();
	SRunner *sr = srunner_create(s);
		
	srunner_run_all(sr, CK_ENV);

	nf = srunner_ntests_failed(sr);

	srunner_free(sr);
	
	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

