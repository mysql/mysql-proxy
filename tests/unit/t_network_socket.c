/* $%BEGINLICENSE%$
 Copyright (C) 2008 MySQL AB, 2008 Sun Microsystems, Inc

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "network-socket.h"

#if GLIB_CHECK_VERSION(2, 16, 0)
#define C(x) x, sizeof(x) - 1

void test_network_socket_new() {
	network_socket *sock;

	sock = network_socket_init();
	g_assert(sock);

	network_socket_free(sock);
}

void test_network_queue_append() {
	network_queue *q;

	q = network_queue_init();
	g_assert(q);

	network_queue_append(q, g_string_new("123"));
	network_queue_append(q, g_string_new("345"));

	network_queue_free(q);
}

void test_network_queue_peek_string() {
	network_queue *q;
	GString *s;

	q = network_queue_init();
	g_assert(q);

	network_queue_append(q, g_string_new("123"));
	g_assert_cmpint(q->len, ==, 3);
	network_queue_append(q, g_string_new("456"));
	g_assert_cmpint(q->len, ==, 6);

	s = network_queue_peek_string(q, 3, NULL);
	g_assert(s);
	g_assert_cmpint(s->len, ==, 3);
	g_assert_cmpstr(s->str, ==, "123");
	g_string_free(s, TRUE);

	s = network_queue_peek_string(q, 4, NULL);
	g_assert(s);
	g_assert_cmpint(s->len, ==, 4);
	g_assert_cmpstr(s->str, ==, "1234");
	g_string_free(s, TRUE);

	s = network_queue_peek_string(q, 7, NULL);
	g_assert(s == NULL);
	
	g_assert_cmpint(q->len, ==, 6);

	network_queue_free(q);
}

void test_network_queue_pop_string() {
	network_queue *q;
	GString *s;

	q = network_queue_init();
	g_assert(q);

	network_queue_append(q, g_string_new("123"));
	g_assert_cmpint(q->len, ==, 3);
	network_queue_append(q, g_string_new("456"));
	g_assert_cmpint(q->len, ==, 6);
	network_queue_append(q, g_string_new("789"));
	g_assert_cmpint(q->len, ==, 9);

	s = network_queue_pop_string(q, 3, NULL);
	g_assert(s);
	g_assert_cmpint(s->len, ==, 3);
	g_assert_cmpstr(s->str, ==, "123");
	g_string_free(s, TRUE);
	
	g_assert_cmpint(q->len, ==, 6);

	s = network_queue_pop_string(q, 4, NULL);
	g_assert(s);
	g_assert_cmpint(s->len, ==, 4);
	g_assert_cmpstr(s->str, ==, "4567");
	g_string_free(s, TRUE);
	g_assert_cmpint(q->len, ==, 2);

	s = network_queue_pop_string(q, 7, NULL);
	g_assert(s == NULL);

	s = network_queue_peek_string(q, 2, NULL);
	g_assert(s);
	g_assert_cmpint(s->len, ==, 2);
	g_assert_cmpstr(s->str, ==, "89");
	g_string_free(s, TRUE);
	g_assert_cmpint(q->len, ==, 2);

	s = network_queue_pop_string(q, 2, NULL);
	g_assert(s);
	g_assert_cmpint(s->len, ==, 2);
	g_assert_cmpstr(s->str, ==, "89");
	g_string_free(s, TRUE);
	g_assert_cmpint(q->len, ==, 0);

	network_queue_free(q);
}

#define TEST_ADDR_IP "127.0.0.1:57684"

void t_network_socket_bind(void) {
	network_socket *sock;
	
	g_log_set_always_fatal(G_LOG_FATAL_MASK); /* we log g_critical() which is fatal for the test-suite */

	sock = network_socket_init();

	/* w/o a address set it should fail */
	g_assert_cmpint(NETWORK_SOCKET_ERROR, ==, network_socket_bind(sock)); /* should fail, no address */

	g_assert_cmpint(0, ==, network_address_set_address(sock->dst, TEST_ADDR_IP));
	
	g_assert_cmpint(NETWORK_SOCKET_SUCCESS, ==, network_socket_bind(sock));

	network_socket_free(sock);

	/* bind again, to test if REUSEADDR works */
	sock = network_socket_init();
	
	g_assert_cmpint(0, ==, network_address_set_address(sock->dst, TEST_ADDR_IP));
	
	g_assert_cmpint(NETWORK_SOCKET_SUCCESS, ==, network_socket_bind(sock));

	g_assert_cmpint(NETWORK_SOCKET_ERROR, ==, network_socket_bind(sock)); /* bind a socket that is already bound, should fail */

	network_socket_free(sock);
}

void t_network_socket_connect(void) {
	network_socket *sock;
	network_socket *client;
	network_socket *client_connected;
	fd_set read_fds;
	struct timeval timeout;
	
	g_log_set_always_fatal(G_LOG_FATAL_MASK); /* we log g_critical() which is fatal for the test-suite */

	sock = network_socket_init();

	g_assert_cmpint(0, ==, network_address_set_address(sock->dst, TEST_ADDR_IP));
	
	g_assert_cmpint(NETWORK_SOCKET_SUCCESS, ==, network_socket_bind(sock));
	
	client = network_socket_init();
	g_assert_cmpint(0, ==, network_address_set_address(client->dst, TEST_ADDR_IP));
	g_assert_cmpint(NETWORK_SOCKET_ERROR_RETRY, ==, network_socket_connect(client)); /* it can't succeed as we don't accept() connections yet */

	client_connected = network_socket_accept(sock);
	g_assert(client_connected);
	
	g_assert_cmpint(NETWORK_SOCKET_SUCCESS, ==, network_socket_connect_finish(client));
	
	/* we are connected */

	network_queue_append(client->send_queue, g_string_new_len(C("foo")));
	g_assert_cmpint(NETWORK_SOCKET_SUCCESS, ==, network_socket_write(client, -1)); /* send all */

	FD_ZERO(&read_fds);
	FD_SET(client_connected->fd, &read_fds);
	timeout.tv_sec = 1;
	timeout.tv_usec = 500 * 000; /* wait 500ms */
	g_assert_cmpint(1, ==, select(client_connected->fd + 1, &read_fds, NULL, NULL, &timeout));
	
	/* socket_read() needs ->to_read set */
	g_assert_cmpint(NETWORK_SOCKET_SUCCESS, ==, network_socket_to_read(client_connected));
	g_assert_cmpint(3, ==, client_connected->to_read);
	g_assert_cmpint(NETWORK_SOCKET_SUCCESS, ==, network_socket_read(client_connected)); /* read all */
	g_assert_cmpint(0, ==, client_connected->to_read);
	
	network_socket_free(client);
	client = NULL;

	/* try to read from closed socket */
	g_assert_cmpint(NETWORK_SOCKET_SUCCESS, ==, network_socket_to_read(client_connected));
	g_assert_cmpint(0, ==, client_connected->to_read);

	network_socket_free(client_connected);
	network_socket_free(sock);
}

int main(int argc, char **argv) {
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/network_socket_new", test_network_socket_new);
	g_test_add_func("/core/network_socket_bind", t_network_socket_bind);
	g_test_add_func("/core/network_socket_connect", t_network_socket_connect);
	g_test_add_func("/core/network_queue_append", test_network_queue_append);
	g_test_add_func("/core/network_queue_peek_string", test_network_queue_peek_string);
	g_test_add_func("/core/network_queue_pop_string", test_network_queue_pop_string);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
