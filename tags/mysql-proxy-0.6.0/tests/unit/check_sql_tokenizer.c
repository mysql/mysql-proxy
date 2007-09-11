#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <check.h>

#include "sql-tokenizer.h"

#define C(x) x, sizeof(x) - 1

START_TEST(test_tokenizer) {
	GPtrArray *tokens = NULL;
	gsize i;

	tokens = g_ptr_array_new();

	sql_tokenizer(tokens, C("SELEcT \"qq-end\"\"\", \"\"\"qq-start\", \"'\"`qq-mixed''\" FROM a AS `b`, `ABC``FOO` "));

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];

#define T(t_id, t_text) \
		fail_unless(token->token_id == t_id, "token[%d].token_id should be '%s', got '%s'", i, sql_token_get_name(t_id), sql_token_get_name(token->token_id)); \
		fail_unless(0 == strcmp(token->text->str, t_text), "token[%d].text should be '%s', got '%s'", i, t_text, token->text->str); \

		switch (i) {
		case 0: T(TK_SQL_SELECT, "SELEcT"); break;
		case 1: T(TK_STRING, "qq-end\""); break;
		case 2: T(TK_COMMA, ","); break;
		case 3: T(TK_STRING, "\"qq-start"); break;
		case 4: T(TK_COMMA, ","); break;
		case 5: T(TK_STRING, "'\"`qq-mixed''"); break;
		case 6: T(TK_SQL_FROM, "FROM"); break;
		case 7: T(TK_LITERAL, "a"); break;
		case 8: T(TK_SQL_AS, "AS"); break;
		case 9: T(TK_LITERAL, "b"); break;
		case 10: T(TK_COMMA, ","); break;
		case 11: T(TK_LITERAL, "ABC`FOO"); break;
#undef T
		default:
			 /**
			  * a self-writing test-case 
			  */
			printf("case %"G_GSIZE_FORMAT": T(%s, \"%s\"); break;\n", i, sql_token_get_name(token->token_id), token->text->str);
			break;
		}
	}

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];

		sql_token_free(token);
	}
	g_ptr_array_free(tokens, TRUE);

	/* cleanup */
} END_TEST

Suite *sql_tokenizer_suite(void) {
	Suite *s = suite_create("sql-tokenizer");
	TCase *tc_core = tcase_create("Core");

	suite_add_tcase (s, tc_core);
	tcase_add_test(tc_core, test_tokenizer);

	return s;
}

int main() {
	int nf;
	Suite *s = sql_tokenizer_suite();
	SRunner *sr = srunner_create(s);
		
	srunner_run_all(sr, CK_ENV);

	nf = srunner_ntests_failed(sr);

	srunner_free(sr);
	
	return (nf == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

