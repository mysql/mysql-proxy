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

