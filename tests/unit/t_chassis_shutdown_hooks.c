/* $%BEGINLICENSE%$
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; version 2 of the
 License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA

 $%ENDLICENSE%$ */
#include <glib.h>

#include "string-len.h"
#include "glib-ext.h"

#include "chassis-shutdown-hooks.h"

void t_chassis_shutdown_hooks_new(void) {
	chassis_shutdown_hooks_t *hooks;

	hooks = chassis_shutdown_hooks_new();

	chassis_shutdown_hooks_free(hooks);
}

/**
 * mock a shutdown function
 *
 * count each execution to proove that it only gets called once 
 */
static void shutdown_func(gpointer _udata) {
	int *count = _udata;

	(*count)++;
}

void t_chassis_shutdown_hooks_register(void) {
	chassis_shutdown_hooks_t *hooks;
	chassis_shutdown_hook_t *hook;
	int count = 0;

	hooks = chassis_shutdown_hooks_new();

	hook = chassis_shutdown_hook_new();
	hook->func = shutdown_func;
	hook->udata = &count;
	g_assert_cmpint(TRUE, ==, chassis_shutdown_hooks_register(hooks,
				C("foo"),
				hook));

	/* check that _call() only execs the shutdown hook once */	
	g_assert_cmpint(hook->is_called, ==, FALSE);
	g_assert_cmpint(*(int *)hook->udata, ==, 0);
	chassis_shutdown_hooks_call(hooks);
	g_assert_cmpint(hook->is_called, ==, TRUE);
	g_assert_cmpint(*(int *)hook->udata, ==, 1);
	chassis_shutdown_hooks_call(hooks);
	g_assert_cmpint(hook->is_called, ==, TRUE);
	g_assert_cmpint(*(int *)hook->udata, ==, 1);

	/* register a hook with the same name */
	hook = chassis_shutdown_hook_new();
	g_assert_cmpint(FALSE, ==, chassis_shutdown_hooks_register(hooks,
				C("foo"),
				hook));
	chassis_shutdown_hook_free(hook); /* as it failed, we have to free it */

	chassis_shutdown_hooks_free(hooks);
}

int main(int argc, char **argv) {
	g_thread_init(NULL);

	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");
	
	g_test_add_func("/chassis/shutdown-hook/init", t_chassis_shutdown_hooks_new);
	g_test_add_func("/chassis/shutdown-hook/register", t_chassis_shutdown_hooks_register);
	
	return g_test_run();
}

