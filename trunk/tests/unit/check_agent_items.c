#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <check.h>

#include "agent_dataitem_spec.h"

#define C(x) x, sizeof(x) - 1

/**
 * Tests for the plugin interface
 * @ingroup plugin
 */

/*@{*/

/**
 * load 
 */
START_TEST(test_agent_dataitem_spec_target) {
	agent_dataitem_spec *s;
	GString *name;

	s = agent_dataitem_spec_init();
	fail_unless(s);

	/* the spec is empty, return NULL */
	name = agent_dataitem_spec_to_dcitem(s);
	fail_unless(name == NULL);

	if (name) g_string_free(name, TRUE);

	agent_dataitem_spec_free(s);
} END_TEST
/*@}*/

Suite *plugin_suite(void) {
	Suite *s = suite_create("dataitems");
	TCase *tc_core = tcase_create("Core");

	suite_add_tcase (s, tc_core);
	tcase_add_test(tc_core, test_agent_dataitem_spec_target);

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

