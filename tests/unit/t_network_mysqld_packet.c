/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "network-mysqld-proto.h"
#include "network-mysqld-packet.h"
#include "glib-ext.h"

#if GLIB_CHECK_VERSION(2, 16, 0)
#define C(x) x, sizeof(x) - 1
#define S(x) x->str, x->len

/**
 * Tests for the MySQL Protocol Codec functions
 * @ingroup proto
 */

/*@{*/
void t_ok_packet_new(void) {
	network_mysqld_ok_packet_t *ok_packet;

	ok_packet = network_mysqld_ok_packet_new();
	g_assert(ok_packet);

	network_mysqld_ok_packet_free(ok_packet);
}

void t_ok_packet_append(void) {
	network_mysqld_ok_packet_t *ok_packet;
	network_packet *packet;

	ok_packet = network_mysqld_ok_packet_new();
	packet = network_packet_new();
	packet->data = g_string_new(NULL);

	/* check if a empty ok-packet is encoded correctly */
	g_assert_cmpint(0, ==, network_mysqld_proto_append_ok_packet(packet->data, ok_packet));
	g_assert_cmpint(7, ==, packet->data->len);
	g_assert_cmpint(0, ==, memcmp(packet->data->str, C("\x00\x00\x00\x00\x00\x00\x00")));

	g_assert_cmpint(0, ==, network_mysqld_proto_get_ok_packet(packet, ok_packet));

	/* check if encoding and decoding works */
	ok_packet->warnings = 1;
	ok_packet->server_status = 2;
	ok_packet->insert_id = 3;
	ok_packet->affected_rows = 4;

	g_string_truncate(packet->data, 0);
	packet->offset = 0;

	g_assert_cmpint(0, ==, network_mysqld_proto_append_ok_packet(packet->data, ok_packet));
	g_assert_cmpint(7, ==, packet->data->len);
	g_assert_cmpint(0, ==, memcmp(packet->data->str, C("\x00\x04\x03\x02\x00\x01\x00")));
	
	network_mysqld_ok_packet_free(ok_packet);

	ok_packet = network_mysqld_ok_packet_new();
	g_assert_cmpint(0, ==, network_mysqld_proto_get_ok_packet(packet, ok_packet));
	g_assert_cmpint(1, ==, ok_packet->warnings);
	g_assert_cmpint(2, ==, ok_packet->server_status);
	g_assert_cmpint(3, ==, ok_packet->insert_id);
	g_assert_cmpint(4, ==, ok_packet->affected_rows);

	network_mysqld_ok_packet_free(ok_packet);

	/* check if too-short packet is denied */
	ok_packet = network_mysqld_ok_packet_new();
	g_string_truncate(packet->data, 0);
	packet->offset = 0;
	g_assert_cmpint(-1, ==, network_mysqld_proto_get_ok_packet(packet, ok_packet));

	network_mysqld_ok_packet_free(ok_packet);

	g_string_free(packet->data, TRUE);
	network_packet_free(packet);
}

void t_err_packet_new(void) {
	network_mysqld_err_packet_t *err_packet;

	err_packet = network_mysqld_err_packet_new();
	g_assert(err_packet);

	network_mysqld_err_packet_free(err_packet);
}

void t_err_packet_append(void) {
	network_mysqld_err_packet_t *err_packet;
	network_packet *packet;

	err_packet = network_mysqld_err_packet_new();
	packet = network_packet_new();
	packet->data = g_string_new(NULL);

	/* check if a empty ok-packet is encoded correctly */
	g_assert_cmpint(0, ==, network_mysqld_proto_append_err_packet(packet->data, err_packet));
	g_assert_cmpint(9, ==, packet->data->len);
	g_assert_cmpint(0, ==, memcmp(packet->data->str, C("\xff\x00\x00#07000")));

	g_assert_cmpint(0, ==, network_mysqld_proto_get_err_packet(packet, err_packet));

	/* check if encoding and decoding works */
	err_packet->errcode = 3;
	g_string_assign_len(err_packet->errmsg, C("test"));
	g_string_assign_len(err_packet->sqlstate, C("01234"));

	g_string_truncate(packet->data, 0);
	packet->offset = 0;

	g_assert_cmpint(0, ==, network_mysqld_proto_append_err_packet(packet->data, err_packet));
	g_assert_cmpint(13, ==, packet->data->len);
	g_assert_cmpint(0, ==, memcmp(packet->data->str, C("\xff\x03\x00#01234test")));
	
	network_mysqld_err_packet_free(err_packet);

	err_packet = network_mysqld_err_packet_new();
	g_assert_cmpint(0, ==, network_mysqld_proto_get_err_packet(packet, err_packet));
	g_assert_cmpint(3, ==, err_packet->errcode);
	g_assert_cmpstr("01234", ==, err_packet->sqlstate->str);
	g_assert_cmpstr("test", ==, err_packet->errmsg->str);

	network_mysqld_err_packet_free(err_packet);

	/* check if too-short packet is denied */
	err_packet = network_mysqld_err_packet_new();
	g_string_truncate(packet->data, 0);
	packet->offset = 0;
	g_assert_cmpint(-1, ==, network_mysqld_proto_get_err_packet(packet, err_packet));

	network_mysqld_err_packet_free(err_packet);

	g_string_free(packet->data, TRUE);
	network_packet_free(packet);
}

void t_eof_packet_new(void) {
	network_mysqld_eof_packet_t *eof_packet;

	eof_packet = network_mysqld_eof_packet_new();
	g_assert(eof_packet);

	network_mysqld_eof_packet_free(eof_packet);
}

void t_eof_packet_append(void) {
	network_mysqld_eof_packet_t *eof_packet;
	network_packet *packet;

	eof_packet = network_mysqld_eof_packet_new();
	packet = network_packet_new();
	packet->data = g_string_new(NULL);

	/* check if a empty ok-packet is encoded correctly */
	g_assert_cmpint(0, ==, network_mysqld_proto_append_eof_packet(packet->data, eof_packet));
	g_assert_cmpint(5, ==, packet->data->len);
	g_assert_cmpint(0, ==, memcmp(packet->data->str, C("\xfe\x00\x00\x00\x00")));

	g_assert_cmpint(0, ==, network_mysqld_proto_get_eof_packet(packet, eof_packet));

	/* check if encoding and decoding works */
	eof_packet->warnings = 1;
	eof_packet->server_status = 2;

	g_string_truncate(packet->data, 0);
	packet->offset = 0;

	g_assert_cmpint(0, ==, network_mysqld_proto_append_eof_packet(packet->data, eof_packet));
	g_assert_cmpint(5, ==, packet->data->len);
	g_assert_cmpint(0, ==, memcmp(packet->data->str, C("\xfe\x01\x00\x02\x00")));
	
	network_mysqld_eof_packet_free(eof_packet);

	eof_packet = network_mysqld_eof_packet_new();
	g_assert_cmpint(0, ==, network_mysqld_proto_get_eof_packet(packet, eof_packet));
	g_assert_cmpint(1, ==, eof_packet->warnings);
	g_assert_cmpint(2, ==, eof_packet->server_status);

	network_mysqld_eof_packet_free(eof_packet);

	/* check if too-short packet is denied */
	eof_packet = network_mysqld_eof_packet_new();
	g_string_truncate(packet->data, 0);
	packet->offset = 0;
	g_assert_cmpint(-1, ==, network_mysqld_proto_get_eof_packet(packet, eof_packet));

	network_mysqld_eof_packet_free(eof_packet);

	g_string_free(packet->data, TRUE);
	network_packet_free(packet);
}

void test_mysqld_handshake(void) {
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
	network_mysqld_auth_challenge *shake;
	network_packet packet;

	shake = network_mysqld_auth_challenge_new();
	
	packet.data = g_string_new(NULL);
	packet.offset = 0;
	g_string_append_len(packet.data, C(raw_packet));

	g_assert_cmpint(packet.data->len, ==, 78);

	g_assert_cmpint(0, ==, network_mysqld_proto_skip_network_header(&packet));
	g_assert_cmpint(0, ==, network_mysqld_proto_get_auth_challenge(&packet, shake));

	g_assert(shake->server_version == 50045);
	g_assert(shake->thread_id == 119);
	g_assert(shake->server_status == 
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

	/* ... and back */
	g_string_truncate(packet.data, 0);
	g_string_append_len(packet.data, C("J\0\0\0"));
	network_mysqld_proto_append_auth_challenge(packet.data, shake);

	g_assert_cmpint(packet.data->len, ==, sizeof(raw_packet) - 1);

	g_assert(0 == memcmp(packet.data->str, raw_packet, packet.data->len));

	network_mysqld_auth_challenge_free(shake);
	g_string_free(packet.data, TRUE);
}

void test_mysqld_auth_empty_pw(void) {
	const char raw_packet[] = 
		"&\0\0\1\205\246\3\0\0\0\0\1\10\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0root\0\0"
		;
	GString *packet;
	network_mysqld_auth_response *auth;
	int i;

	auth = network_mysqld_auth_response_new();
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

	g_assert(0 == network_mysqld_proto_append_auth_response(packet, auth));

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

	network_mysqld_auth_response_free(auth);

	g_string_free(packet, TRUE);
}

void test_mysqld_auth_with_pw(void) {
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
	network_mysqld_auth_response *auth;
	int i;

	auth = network_mysqld_auth_response_new();
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

	g_assert(0 == network_mysqld_proto_append_auth_response(packet, auth));
	g_assert(packet->len == sizeof(raw_packet) - 1);

#if 0
	for (i = 0; i < packet->len; i++) {
		g_message("%s: [%d] %02x %c= %02x", G_STRLOC, i, packet->str[i], packet->str[i] == raw_packet[i] ? '=' : '!', raw_packet[i]);
	}
#endif

	g_assert(0 == memcmp(packet->str, raw_packet, sizeof(raw_packet) - 1));

	network_mysqld_auth_response_free(auth);

	g_string_free(packet, TRUE);
	g_string_free(challenge, TRUE);
}

void t_auth_response_new() {
	network_mysqld_auth_response *shake;

	shake = network_mysqld_auth_response_new();

	network_mysqld_auth_response_free(shake);
}

void t_mysqld_get_auth_response(void) {
	const char raw_packet[] = 
		"\205\246\3\0"
		"\0\0\0\1"
		"\10"
		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
		"root\0"
		"\24\241\304\260>\255\1:F,\256\337K\323\340\4\273\354I\256\204"
		;

	network_mysqld_auth_response *auth;
	network_packet packet;
	int err = 0;

	auth = network_mysqld_auth_response_new();
	packet.data = g_string_new_len(C(raw_packet));
	packet.offset = 0;
	
	err = err || network_mysqld_proto_get_auth_response(&packet, auth);

	g_assert_cmpint(err, ==, 0);

	g_assert(auth->username);
	g_assert_cmpint(auth->username->len, ==, 4);
	g_assert_cmpstr(auth->username->str, ==, "root");

	g_assert_cmpuint(auth->capabilities, ==,
		CLIENT_LONG_PASSWORD |
	       	CLIENT_LONG_FLAG |
		CLIENT_LOCAL_FILES | 
		CLIENT_PROTOCOL_41 |
		CLIENT_INTERACTIVE |
		CLIENT_TRANSACTIONS |
		CLIENT_SECURE_CONNECTION |
		CLIENT_MULTI_STATEMENTS |
		CLIENT_MULTI_RESULTS); 
	g_assert_cmpuint(auth->max_packet_size, ==, 1 << 24);
	g_assert_cmpuint(auth->charset        , ==, 8);

	g_string_truncate(packet.data, 0);
	packet.offset = 0;

	err = err || network_mysqld_proto_append_auth_response(packet.data, auth);
	g_assert_cmpint(err, ==, 0);

	g_assert_cmpint(packet.data->len, ==, sizeof(raw_packet) - 1);
	g_assert_cmpint(0, ==, memcmp(packet.data->str, raw_packet, packet.data->len));

	network_mysqld_auth_response_free(auth);

	/* empty auth struct */
	g_string_truncate(packet.data, 0);
	packet.offset = 0;

	auth = network_mysqld_auth_response_new();
	err = err || network_mysqld_proto_append_auth_response(packet.data, auth);
	g_assert_cmpint(err, ==, 0);
	network_mysqld_auth_response_free(auth);

	g_string_free(packet.data, TRUE);
}



int main(int argc, char **argv) {
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/ok-packet-new", t_ok_packet_new);
	g_test_add_func("/core/ok-packet-append", t_ok_packet_append);
	g_test_add_func("/core/eof-packet-new", t_eof_packet_new);
	g_test_add_func("/core/eof-packet-append", t_eof_packet_append);
	g_test_add_func("/core/err-packet-new", t_err_packet_new);
	g_test_add_func("/core/err-packet-append", t_err_packet_append);

	g_test_add_func("/core/mysqld-proto-handshake", test_mysqld_handshake);

	g_test_add_func("/core/mysqld-proto-pw-empty", test_mysqld_auth_empty_pw);
	g_test_add_func("/core/mysqld-proto-pw", test_mysqld_auth_with_pw);
	
	g_test_add_func("/core/mysqld-proto-auth-response-new", t_auth_response_new);
	g_test_add_func("/core/mysqld-proto-get-auth-response", t_mysqld_get_auth_response);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
