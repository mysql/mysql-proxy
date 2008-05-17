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

#include "network-socket.h"

#if GLIB_CHECK_VERSION(2, 16, 0)
#define C(x) x, sizeof(x) - 1

void test_network_socket_new() {
	network_socket *sock;

	sock = network_socket_init();

	network_socket_free(sock);
}

int main(int argc, char **argv) {
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/network_socket_new", test_network_socket_new);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
