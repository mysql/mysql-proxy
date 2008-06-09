/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "network-mysqld-proto.h"
#include "network-mysqld-binlog.h"
#include "glib-ext.h"

#if GLIB_CHECK_VERSION(2, 16, 0)
#define C(x) x, sizeof(x) - 1

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
void test_mysqld_proto_header(void) {
	unsigned char header[4];
	size_t length = 1256;

	g_assert(0 == network_mysqld_proto_set_header(header, length, 0));
	g_assert(length == network_mysqld_proto_get_header(header));
}

/**
 * @test network_mysqld_proto_append_lenenc_int() and network_mysqld_proto_get_lenenc_int()
 *
 */
void test_mysqld_proto_lenenc_int(void) {
	guint64 length;
	network_packet packet;

	packet.data = g_string_new(NULL);

	/* we should be able to do more corner case testing
	 *
	 *
	 */
	length = 0; packet.offset = 0;
	g_string_truncate(packet.data, 0);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet.data, length));
	g_assert(packet.data->len == 1);
	g_assert(length == network_mysqld_proto_get_lenenc_int(&packet));

	length = 250; packet.offset = 0;
	g_string_truncate(packet.data, 0);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet.data, length));
	g_assert(packet.data->len == 1);
	g_assert(length == network_mysqld_proto_get_lenenc_int(&packet));

	length = 251; packet.offset = 0;
	g_string_truncate(packet.data, 0);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet.data, length));
	g_assert(packet.data->len == 3);
	g_assert(length == network_mysqld_proto_get_lenenc_int(&packet));

	length = 0xffff; packet.offset = 0;
	g_string_truncate(packet.data, 0);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet.data, length));
	g_assert(packet.data->len == 3);
	g_assert(length == network_mysqld_proto_get_lenenc_int(&packet));

	length = 0x10000; packet.offset = 0;
	g_string_truncate(packet.data, 0);
	g_assert(0 == network_mysqld_proto_append_lenenc_int(packet.data, length));
	g_assert(packet.data->len == 4);
	g_assert(length == network_mysqld_proto_get_lenenc_int(&packet));

	g_string_free(packet.data, TRUE);
}

/**
 * @test network_mysqld_proto_append_lenenc_int() and network_mysqld_proto_get_lenenc_int()
 *
 */
void test_mysqld_proto_int(void) {
	guint64 length;
	network_packet packet;

	packet.data = g_string_new(NULL);
	/* we should be able to do more corner case testing
	 *
	 *
	 */

	length = 0; packet.offset = 0;
	g_string_truncate(packet.data, 0);
	g_assert(0 == network_mysqld_proto_append_int8(packet.data, length));
	g_assert(packet.data->len == 1);
	g_assert(length == network_mysqld_proto_get_int8(&packet));

	length = 0; packet.offset = 0;
	g_string_truncate(packet.data, 0);
	g_assert(0 == network_mysqld_proto_append_int16(packet.data, length));
	g_assert(packet.data->len == 2);
	g_assert(length == network_mysqld_proto_get_int16(&packet));

	length = 0; packet.offset = 0;
	g_string_truncate(packet.data, 0);
	g_assert(0 == network_mysqld_proto_append_int32(packet.data, length));
	g_assert(packet.data->len == 4);
	g_assert(length == network_mysqld_proto_get_int32(&packet));

	g_string_free(packet.data, TRUE);
}
/*@}*/

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
	g_string_append_len(packet.data, raw_packet, sizeof(raw_packet) - 1);

	g_assert(packet.data->len == 78);

	g_assert(0 == network_mysqld_proto_get_auth_challenge(&packet, shake));

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

void test_mysqld_binlog_events(void) {
	/**
	 * decoding the binlog packet
	 *
	 * - http://dev.mysql.com/doc/internals/en/replication-common-header.html
	 *
	 */

	const char rotate_packet[] =
		"/\0\0\1"
		  "\0"        /* OK */
		   "\0\0\0\0" /* timestamp */
		   "\4"       /* ROTATE */
		   "\1\0\0\0" /* server-id */
		   ".\0\0\0"  /* event-size */
		   "\0\0\0\0" /* log-pos */
		   "\0\0"     /* flags */
		   "f\0\0\0\0\0\0\0hostname-bin.000009";

	const char format_packet[] =
		"c\0\0\2"
		  "\0"
		    "F\335\6F" /* timestamp */
		    "\17"      /* FORMAT_DESCRIPTION_EVENT */
		    "\1\0\0\0" /* server-id */
		    "b\0\0\0"  /* event-size */
		    "\0\0\0\0" /* log-pos */
		    "\0\0"     /* flags */
		    "\4\0005.1.16-beta-log\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0238\r\0\10\0\22\0\4\4\4\4\22\0\0O\0\4\32\10\10\10\10"; /* */

	const char query_packet[] = 
		"N\0\0\3"
		  "\0"          
		    "g\255\7F"   /* timestamp */
		    "\2"         /* QUERY_EVENT */
		    "\1\0\0\0"   /* server-id */
		    "M\0\0\0"    /* event-size */
		    "\263\0\0\0" /* log-pos */
		    "\20\0"      /* flags */
		      "\2\0\0\0" /* thread-id */
		      "\0\0\0\0" /* query-time */
		      "\5"       /* str-len of default-db (world) */
		      "\0\0"     /* error-code on master-side */
		        "\32\0"  /* var-size-len (5.0 and later) */
		          "\0"   /* Q_FLAGS2_CODE */
		            "\0@\0\0" /* flags (4byte) */
		          "\1"   /* Q_SQL_MODE_CODE */
		            "\0\0\0\0\0\0\0\0" /* (8byte) */
		          "\6"   /* Q_CATALOG_NZ_CODE */
		            "\3std" /* (4byte) */
		          "\4"   /* Q_CHARSET_CODE */
		            "\10\0\10\0\10\0" /* (6byte) */
		          "world\0"
		          "drop table t1";

	network_mysqld_binlog *binlog;
	network_mysqld_binlog_event *event;
	network_packet *packet;

	/* rotate event */

	binlog = network_mysqld_binlog_new();
	event = network_mysqld_binlog_event_new();
	packet = network_packet_new();
	packet->data = g_string_new(NULL);

	g_string_assign_len(packet->data, C(rotate_packet));

	network_mysqld_proto_skip_network_header(packet);
	network_mysqld_proto_get_binlog_status(packet);
	network_mysqld_proto_get_binlog_event_header(packet, event);

	g_assert_cmpint(event->event_type, ==, ROTATE_EVENT);

	network_mysqld_proto_get_binlog_event(packet, binlog, event);

	g_assert_cmpint(event->event.rotate_event.binlog_pos, ==, 102);
	g_assert_cmpstr(event->event.rotate_event.binlog_file, ==, "hostname-bin.000009");

	g_string_free(packet->data, TRUE);
	network_packet_free(packet);
	network_mysqld_binlog_event_free(event);
	network_mysqld_binlog_free(binlog);

	/* format description */

	binlog = network_mysqld_binlog_new();
	event = network_mysqld_binlog_event_new();
	packet = network_packet_new();
	packet->data = g_string_new(NULL);

	g_string_assign_len(packet->data, C(format_packet));


	network_mysqld_proto_skip_network_header(packet);
	network_mysqld_proto_get_binlog_status(packet);

	network_mysqld_proto_get_binlog_event_header(packet, event);
	g_assert_cmpint(event->event_type, ==, FORMAT_DESCRIPTION_EVENT);

	network_mysqld_proto_get_binlog_event(packet, binlog, event);

	g_string_free(packet->data, TRUE);
	network_packet_free(packet);
	network_mysqld_binlog_event_free(event);
	network_mysqld_binlog_free(binlog);

	/* query */

	binlog = network_mysqld_binlog_new();
	event = network_mysqld_binlog_event_new();
	packet = network_packet_new();
	packet->data = g_string_new(NULL);

	g_string_assign_len(packet->data, C(query_packet));

	network_mysqld_proto_skip_network_header(packet);
	network_mysqld_proto_get_binlog_status(packet);
	network_mysqld_proto_get_binlog_event_header(packet, event);

	g_assert_cmpint(event->event_type, ==, QUERY_EVENT);

	network_mysqld_proto_get_binlog_event(packet, binlog, event);
	g_assert_cmpstr(event->event.query_event.db_name, ==, "world");
	g_assert_cmpstr(event->event.query_event.query, ==, "drop table t1");

	g_string_free(packet->data, TRUE);
	network_packet_free(packet);
	network_mysqld_binlog_event_free(event);
	network_mysqld_binlog_free(binlog);
}

void t_auth_response_new() {
	network_mysqld_auth_response *shake;

	shake = network_mysqld_auth_response_new();

	network_mysqld_auth_response_free(shake);
}

void t_mysqld_get_auth_response(void) {
	const char raw_packet[] = 
		":\0\0\1"
		"\205\246\3\0"
		"\0\0\0\1"
		"\10"
		"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
		"root\0"
		"\24\241\304\260>\255\1:F,\256\337K\323\340\4\273\354I\256\204"
		;

	network_mysqld_auth_response *auth;
	network_packet packet;

	auth = network_mysqld_auth_response_new();
	packet.data = g_string_new_len(C(raw_packet));
	packet.offset = 0;
	
	network_mysqld_proto_skip_network_header(&packet); /* packet-header */

	network_mysqld_proto_get_auth_response(&packet, auth);

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

	network_mysqld_auth_response_free(auth);

	g_string_free(packet.data, TRUE);
}


int main(int argc, char **argv) {
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/mysqld-proto-header", test_mysqld_proto_header);
	g_test_add_func("/core/mysqld-proto-lenenc-int", test_mysqld_proto_lenenc_int);
	g_test_add_func("/core/mysqld-proto-int", test_mysqld_proto_int);
	
	g_test_add_func("/core/mysqld-proto-handshake", test_mysqld_handshake);

	g_test_add_func("/core/mysqld-proto-pw-empty", test_mysqld_auth_empty_pw);
	g_test_add_func("/core/mysqld-proto-pw", test_mysqld_auth_with_pw);
	
	g_test_add_func("/core/mysqld-proto-binlog-event", test_mysqld_binlog_events);
	g_test_add_func("/core/mysqld-proto-auth-response-new", t_auth_response_new);
	g_test_add_func("/core/mysqld-proto-get-auth-response", t_mysqld_get_auth_response);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
