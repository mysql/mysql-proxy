#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <check.h>

#include "network-mysqld-proto.h"

#define C(x) x, sizeof(x) - 1

/**
 * @test network_mysqld_proto_set_header() and network_mysqld_proto_get_header()
 *
 * how to handle > 16M ?
 */
START_TEST(test_mysqld_proto_header) {
	unsigned char header[4];
	size_t length = 1256;

	fail_unless(0 == network_mysqld_proto_set_header(header, length, 0));
	fail_unless(length == network_mysqld_proto_get_header(header));
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
	fail_unless(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	fail_unless(packet->len == 1);
	fail_unless(length == network_mysqld_proto_get_lenenc_int(packet, &off));

	length = 250; off = 0;
	g_string_truncate(packet, off);
	fail_unless(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	fail_unless(packet->len == 1);
	fail_unless(length == network_mysqld_proto_get_lenenc_int(packet, &off));

	length = 251; off = 0;
	g_string_truncate(packet, off);
	fail_unless(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	fail_unless(packet->len == 3);
	fail_unless(length == network_mysqld_proto_get_lenenc_int(packet, &off));

	length = 0xffff; off = 0;
	g_string_truncate(packet, off);
	fail_unless(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	fail_unless(packet->len == 3);
	fail_unless(length == network_mysqld_proto_get_lenenc_int(packet, &off));

	length = 0x10000; off = 0;
	g_string_truncate(packet, off);
	fail_unless(0 == network_mysqld_proto_append_lenenc_int(packet, length));
	fail_unless(packet->len == 4);
	fail_unless(length == network_mysqld_proto_get_lenenc_int(packet, &off));

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
	fail_unless(0 == network_mysqld_proto_append_int8(packet, length));
	fail_unless(packet->len == 1);
	fail_unless(length == network_mysqld_proto_get_int8(packet, &off));

	length = 0; off = 0;
	g_string_truncate(packet, off);
	fail_unless(0 == network_mysqld_proto_append_int16(packet, length));
	fail_unless(packet->len == 2);
	fail_unless(length == network_mysqld_proto_get_int16(packet, &off));

	length = 0; off = 0;
	g_string_truncate(packet, off);
	fail_unless(0 == network_mysqld_proto_append_int32(packet, length));
	fail_unless(packet->len == 4);
	fail_unless(length == network_mysqld_proto_get_int32(packet, &off));

	g_string_free(packet, TRUE);
} END_TEST

Suite *mysqld_proto_suite(void) {
	Suite *s = suite_create("mysqld_proto");
	TCase *tc_core = tcase_create("Core");

	suite_add_tcase (s, tc_core);
	tcase_add_test(tc_core, test_mysqld_proto_header);
	tcase_add_test(tc_core, test_mysqld_proto_lenenc_int);
	tcase_add_test(tc_core, test_mysqld_proto_int);

	return s;
}

int main() {
	int nf;
	Suite *s = mysqld_proto_suite();
	SRunner *sr = srunner_create(s);
		
	srunner_run_all(sr, CK_ENV);

	nf = srunner_ntests_failed(sr);

	srunner_free(sr);
	
	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

