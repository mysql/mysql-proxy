#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <check.h>

#include "cauldron-plugin.h"

#define C(x) x, sizeof(x) - 1

/**
 * Tests for the plugin interface
 * @ingroup plugin
 */

/* create a dummy plugin */

struct cauldron_plugin_config {
	gchar *foo;
};

cauldron_plugin_config *mock_plugin_init(void) {
	cauldron_plugin_config *config;

	config = g_new0(cauldron_plugin_config, 1);

	return config;
}

void mock_plugin_destroy(cauldron_plugin_config *config) {
	if (config->foo) g_free(config->foo);

	g_free(config);
}

int mock_plugin_add_options(GOptionContext *option_ctx, cauldron_plugin_config *config) {
	GOptionGroup *option_grp;
	guint i;

	GOptionEntry config_entries[] = 
	{
		{ "foo", 0, 0, G_OPTION_ARG_STRING, NULL, "foo", "foo" },
		
		{ NULL,  0, 0, G_OPTION_ARG_NONE,   NULL, NULL, NULL }
	};

	i = 0;
	config_entries[i++].arg_data = &(config->foo);

	option_grp = g_option_group_new("foo", "foo-module", "Show options for the foo-module", NULL, NULL);
	g_option_group_add_entries(option_grp, config_entries);
	g_option_context_add_group(option_ctx, option_grp);

	return 0;
}

static void devnull_log_func(const gchar *log_domain, GLogLevelFlags log_level, const gchar *message, gpointer user_data) {
	/* discard the output */
}


/*@{*/

/**
 * load 
 */
START_TEST(test_plugin_load) {
	cauldron_plugin *p;
	GLogFunc old_log_func;

	p = cauldron_plugin_init();
	fail_unless(p != NULL);
	cauldron_plugin_free(p);

	old_log_func = g_log_set_default_handler(devnull_log_func, NULL);
	/** should fail */
	p = cauldron_plugin_load("non-existing");
	g_log_set_default_handler(old_log_func, NULL);
	fail_unless(p == NULL);
	if (p != NULL) cauldron_plugin_free(p);
} END_TEST

START_TEST(test_plugin_config) {
	cauldron_plugin *p;
	GOptionContext *option_ctx;
	gchar **_argv;
	int _argc = 2;
	GError *gerr = NULL;
	
	p = cauldron_plugin_init();
	p->init = mock_plugin_init;
	p->destroy = mock_plugin_destroy;
	p->add_options = mock_plugin_add_options;

	p->config = p->init();
	fail_unless(p->config != NULL);
	
	_argv = g_new(char *, 2);
	_argv[0] = g_strdup("test_plugin_config");
	_argv[1] = g_strdup("--foo=123");

	/* set some config variables */
	option_ctx = g_option_context_new("- MySQL Proxy");
	fail_unless(0 == p->add_options(option_ctx, p->config));
	fail_unless(FALSE != g_option_context_parse(option_ctx, &_argc, &_argv, &gerr));
	g_option_context_free(option_ctx);

	fail_unless(0 == strcmp(p->config->foo, "123"));

	g_free(_argv[1]);

	/* unknown option, let it fail */
	_argv[1] = g_strdup("--fo");
	_argc = 2;

	option_ctx = g_option_context_new("- MySQL Proxy");
	fail_unless(0 == p->add_options(option_ctx, p->config));
	fail_unless(FALSE == g_option_context_parse(option_ctx, &_argc, &_argv, &gerr));
	fail_unless(gerr->domain == G_OPTION_ERROR);
	g_error_free(gerr); gerr = NULL;

	g_option_context_free(option_ctx);

	p->destroy(p->config);
	cauldron_plugin_free(p);


	/* let's try again, just let it fail */
} END_TEST
/*@}*/

Suite *plugin_suite(void) {
	Suite *s = suite_create("plugin");
	TCase *tc_core = tcase_create("Core");

	suite_add_tcase (s, tc_core);
	tcase_add_test(tc_core, test_plugin_load);
	tcase_add_test(tc_core, test_plugin_config);

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

