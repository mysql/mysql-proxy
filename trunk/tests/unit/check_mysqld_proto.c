/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "network-mysqld-proto.h"

#if GLIB_CHECK_VERSION(2, 16, 0)
#define C(x) x, sizeof(x) - 1

#define START_TEST(x) void (x)(void)
#define END_TEST


/**
 * Tests for the MySQL Protocol Codec functions
 * @ingroup proto
 */

/*@{*/

/**
 * @test network_mysqld_proto_set_header() and network_mysqld_proto_get_header()
 *
 * how to handle > 16M ?
 */
START_TEST(test_mysqld_proto_header) {
	unsigned char header[4];
	size_t length = 1256;

	g_assert(0 == network_mysqld_proto_set_header(header, length, 0));
	g_assert(length == network_mysqld_proto_get_header(header));
} END_TEST

/**
 * @test network_mysqld_proto_append_lenenc_int() and network_mysqld_proto_get_lenenc_int()
 *
 */
START_TEST(test_mysqld_proto_lenenc_int) {
	GString *packet = g_string_new(NULL);
	guint64 length;
	guint off;

	/* we should be able to do more corner case testing
	 *
	 *
	 */
	length = 0; off = 0;
	g_string_truncate(packet, off);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	g_assert(packet->len == 1);
	g_assert(length == network_mysqld_proto_get_lenenc_int(packet, &off));

	length = 250; off = 0;
	g_string_truncate(packet, off);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	g_assert(packet->len == 1);
	g_assert(length == network_mysqld_proto_get_lenenc_int(packet, &off));

	length = 251; off = 0;
	g_string_truncate(packet, off);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	g_assert(packet->len == 3);
	g_assert(length == network_mysqld_proto_get_lenenc_int(packet, &off));

	length = 0xffff; off = 0;
	g_string_truncate(packet, off);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	g_assert(packet->len == 3);
	g_assert(length == network_mysqld_proto_get_lenenc_int(packet, &off));

	length = 0x10000; off = 0;
	g_string_truncate(packet, off);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	g_assert(packet->len == 4);
	g_assert(length == network_mysqld_proto_get_lenenc_int(packet, &off));

	g_string_free(packet, TRUE);
} END_TEST

/**
 * @test network_mysqld_proto_append_lenenc_int() and network_mysqld_proto_get_lenenc_int()
 *
 */
START_TEST(test_mysqld_proto_int) {
	GString *packet = g_string_new(NULL);
	guint64 length;
	guint off;

	/* we should be able to do more corner case testing
	 *
	 *
	 */

	length = 0; off = 0;
	g_string_truncate(packet, off);
	g_assert(0 == network_mysqld_proto_append_int8(packet, length));
	g_assert(packet->len == 1);
	g_assert(length == network_mysqld_proto_get_int8(packet, &off));

	length = 0; off = 0;
	g_string_truncate(packet, off);
	g_assert(0 == network_mysqld_proto_append_int16(packet, length));
	g_assert(packet->len == 2);
	g_assert(length == network_mysqld_proto_get_int16(packet, &off));

	length = 0; off = 0;
	g_string_truncate(packet, off);
	g_assert(0 == network_mysqld_proto_append_int32(packet, length));
	g_assert(packet->len == 4);
	g_assert(length == network_mysqld_proto_get_int32(packet, &off));

	g_string_free(packet, TRUE);
} END_TEST
/*@}*/

START_TEST(test_mysqld_handshake) {
	const char raw_packet[] = "J\0\0\0"
		"\n"
		"5.0.45-Debian_1ubuntu3.3-log\0"
		"w\0\0\0"
		"\"L;!3|8@"
		"\0"
		",\242" /* 0x2c 0xa2 */
		"\10"
		"\2\0"
		"\0\0\0\0\0\0\0\0\0\0\0\0\0"
		"vV,s#PLjSA+Q"
		"\0";
	GString *packet;
	network_mysqld_handshake *shake;

	shake = network_mysqld_handshake_new();
	
	packet = g_string_new(NULL);
	g_string_append_len(packet, raw_packet, sizeof(raw_packet) - 1);

	g_assert(packet->len == 78);

	g_assert(0 == network_mysqld_proto_get_handshake(packet, shake));

	g_assert(shake->server_version == 50045);
	g_assert(shake->thread_id == 119);
	g_assert(shake->status == 
			SERVER_STATUS_AUTOCOMMIT);
	g_assert(shake->charset == 8);
	g_assert(shake->capabilities ==
			(CLIENT_CONNECT_WITH_DB |
			CLIENT_LONG_FLAG |

			CLIENT_COMPRESS |

			CLIENT_PROTOCOL_41 |

			CLIENT_TRANSACTIONS |
			CLIENT_SECURE_CONNECTION));

	g_assert(shake->challenge->len == 20);
	g_assert(0 == memcmp(shake->challenge->str, "\"L;!3|8@vV,s#PLjSA+Q", shake->challenge->len));

	network_mysqld_handshake_free(shake);
	g_string_free(packet, TRUE);
} END_TEST

START_TEST(test_mysqld_auth_empty_pw) {
	const char raw_packet[] = 
		"&\0\0\1\205\246\3\0\0\0\0\1\10\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0root\0\0"
		;
	GString *packet;
	network_mysqld_auth *auth;
	int i;

	auth = network_mysqld_auth_new();
	g_string_assign(auth->username, "root");
	auth->capabilities    = 
		(CLIENT_LONG_PASSWORD |
	       	CLIENT_LONG_FLAG |
		CLIENT_LOCAL_FILES | 
		CLIENT_PROTOCOL_41 |
		CLIENT_INTERACTIVE |
		CLIENT_TRANSACTIONS |
		CLIENT_SECURE_CONNECTION |
		CLIENT_MULTI_STATEMENTS |
		CLIENT_MULTI_RESULTS); 
	auth->max_packet_size = 1 << 24;
	auth->charset         = 8;
	
	packet = g_string_new(NULL);

	network_mysqld_proto_append_int8(packet, 38);
	network_mysqld_proto_append_int8(packet, 0);
	network_mysqld_proto_append_int8(packet, 0);
	network_mysqld_proto_append_int8(packet, 1);

	g_assert(0 == network_mysqld_proto_append_auth(packet, auth));

#if 0
	g_message("%s: packet->len = %d, packet is: %d", G_STRLOC, packet->len, sizeof(raw_packet) - 1);
#endif

	g_assert(packet->len == sizeof(raw_packet) - 1);

#if 0
	for (i = 0; i < packet->len; i++) {
		g_message("%s: [%d] %02x %c= %02x", G_STRLOC, i, packet->str[i], packet->str[i] == raw_packet[i] ? '=' : '!', raw_packet[i]);
	}
#endif

	g_assert(0 == memcmp(packet->str, raw_packet, sizeof(raw_packet) - 1));

	network_mysqld_auth_free(auth);

	g_string_free(packet, TRUE);
} END_TEST

START_TEST(test_mysqld_auth_with_pw) {
	const char raw_packet[] = 
		":\0\0\1"
		"\205\246\3\0"
		"\0\0\0\1"
		"\10"
		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
		"root\0"
		"\24\241\304\260>\255\1:F,\256\337K\323\340\4\273\354I\256\204"
		;
	const char raw_challenge[] = 
			"%@R[SoWC"      /* part 1 */
			"+L|LG_+R={tV"; /* part 2 */

	GString *packet, *challenge;
	network_mysqld_auth *auth;
	int i;

	auth = network_mysqld_auth_new();
	g_string_assign(auth->username, "root");
	auth->capabilities    = 
		CLIENT_LONG_PASSWORD |
	       	CLIENT_LONG_FLAG |
		CLIENT_LOCAL_FILES | 
		CLIENT_PROTOCOL_41 |
		CLIENT_INTERACTIVE |
		CLIENT_TRANSACTIONS |
		CLIENT_SECURE_CONNECTION |
		CLIENT_MULTI_STATEMENTS |
		CLIENT_MULTI_RESULTS; 
	auth->max_packet_size = 1 << 24;
	auth->charset         = 8;

	challenge = g_string_new(NULL);
	g_string_append_len(challenge, raw_challenge, sizeof(raw_challenge) - 1);

	network_mysqld_proto_scramble(auth->response, challenge, "123");
	
	packet = g_string_new(NULL);

	network_mysqld_proto_append_int8(packet, 58);
	network_mysqld_proto_append_int8(packet, 0);
	network_mysqld_proto_append_int8(packet, 0);
	network_mysqld_proto_append_int8(packet, 1);

	g_assert(0 == network_mysqld_proto_append_auth(packet, auth));
	g_assert(packet->len == sizeof(raw_packet) - 1);

#if 0
	for (i = 0; i < packet->len; i++) {
		g_message("%s: [%d] %02x %c= %02x", G_STRLOC, i, packet->str[i], packet->str[i] == raw_packet[i] ? '=' : '!', raw_packet[i]);
	}
#endif

	g_assert(0 == memcmp(packet->str, raw_packet, sizeof(raw_packet) - 1));

	network_mysqld_auth_free(auth);

	g_string_free(packet, TRUE);
} END_TEST

int main(int argc, char **argv) {
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/mysqld-proto-header", test_mysqld_proto_header);
	g_test_add_func("/core/mysqld-proto-lenenc-int", test_mysqld_proto_lenenc_int);
	g_test_add_func("/core/mysqld-proto-int", test_mysqld_proto_int);
	
	g_test_add_func("/core/mysqld-proto-handshake", test_mysqld_handshake);

	g_test_add_func("/core/mysqld-proto-pw-empty", test_mysqld_auth_empty_pw);
	g_test_add_func("/core/mysqld-proto-pw", test_mysqld_auth_with_pw);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
