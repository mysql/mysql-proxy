#include <glib.h>

#include "network-backend.h"

#if GLIB_CHECK_VERSION(2, 16, 0)
#define C(x) x, sizeof(x) - 1

/**
 * test of network_backends_new() allocates a memory 
 */
void t_network_backends_new() {
	network_backends_t *backends;

	backends = network_backends_new();
	g_assert(backends);

	g_assert_cmpint(network_backends_count(backends), ==, 0);

	/* free against a empty pool */
	network_backends_free(backends);
}

void t_network_backend_new() {
	backend_t *b;

	b = backend_init();
	g_assert(b);

	backend_free(b);
}

void t_network_backends_add() {
	network_backends_t *backends;

	backends = network_backends_new();
	g_assert(backends);
	
	g_assert_cmpint(network_backends_count(backends), ==, 0);

	/* insert should work */
	g_assert_cmpint(network_backends_add(backends, "127.0.0.1", BACKEND_TYPE_RW), ==, 0);
	
	g_assert_cmpint(network_backends_count(backends), ==, 1);

	/* is duplicate, should fail */
	g_assert_cmpint(network_backends_add(backends, "127.0.0.1", BACKEND_TYPE_RW), !=, 0);
	
	g_assert_cmpint(network_backends_count(backends), ==, 1);

	/* unfolds to the same default */
	g_assert_cmpint(network_backends_add(backends, "127.0.0.1:3306", BACKEND_TYPE_RW), !=, 0);
	
	g_assert_cmpint(network_backends_count(backends), ==, 1);

	network_backends_free(backends);
}

int main(int argc, char **argv) {
	g_thread_init(NULL);
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/network_backends_new", t_network_backends_new);
	g_test_add_func("/core/network_backend_new", t_network_backend_new);
	g_test_add_func("/core/network_backends_add", t_network_backends_add);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
