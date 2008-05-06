/* Copyright (C) 2007, 2008 MySQL AB */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "sql-tokenizer.h"

#if GLIB_CHECK_VERSION(2, 16, 0)
#define C(x) x, sizeof(x) - 1

#define START_TEST(x) void (x)(void)
#define END_TEST

/** 
 * tests for the SQL tokenizer
 * @ingroup sql test
 * @{
 */

/**
 * @test check if SQL tokenizing works
 *  
 */
START_TEST(test_tokenizer) {
	GPtrArray *tokens = NULL;
	gsize i;

	tokens = sql_tokens_new();

	sql_tokenizer(tokens, C("SELEcT \"qq-end\"\"\", \"\"\"qq-start\", \"'\"`qq-mixed''\" FROM a AS `b`, `ABC``FOO` "));

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];

#define T(t_id, t_text) \
		g_assert_cmpint(token->token_id, ==, t_id); \
		g_assert_cmpstr(token->text->str, ==, t_text); 

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

	/* cleanup */
	sql_tokens_free(tokens);
} END_TEST

/**
 * @test table-names might start with a _ even without quoting
 *  
 */
START_TEST(test_table_name_underscore) {
	GPtrArray *tokens = NULL;
	gsize i;

	tokens = sql_tokens_new();

	sql_tokenizer(tokens, C("SELEcT * FROM __test_table "));

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];

#define T(t_id, t_text) \
		g_assert_cmpint(token->token_id, ==, t_id); \
		g_assert_cmpstr(token->text->str, ==, t_text);

		switch (i) {
		case 0: T(TK_SQL_SELECT, "SELEcT"); break;
		case 1: T(TK_STAR, "*"); break;
		case 2: T(TK_SQL_FROM, "FROM"); break;
		case 3: T(TK_LITERAL, "__test_table"); break;
#undef T
		default:
			 /**
			  * a self-writing test-case 
			  */
			printf("case %"G_GSIZE_FORMAT": T(%s, \"%s\"); break;\n", i, sql_token_get_name(token->token_id), token->text->str);
			break;
		}
	}

	/* cleanup */
	sql_tokens_free(tokens);
} END_TEST


/**
 * @test check if we can map all tokens to a name and back again
 *   
 */
START_TEST(test_token2name) {
	gsize i;

	/* convert tokens to id and back to name */
	for (i = 0; i < TK_LAST_TOKEN; i++) {
		const char *name;

		g_assert((name = sql_token_get_name(i)));
	}
} END_TEST

/**
 * @test check if we can map all tokens to a name and back again
 *   
 */
START_TEST(test_keyword2token) {
	gsize i;

	const struct {
		const char *token;
		sql_token_id id;
	} keywords[] = {
		{ "SELECT", TK_SQL_SELECT },
		{ "INSERT", TK_SQL_INSERT },

		{ NULL, TK_UNKNOWN }
	};

	/* convert tokens to id and back to name */
	for (i = 0; keywords[i].token; i++) {
		g_assert_cmpint(keywords[i].id, ==, sql_token_get_id(keywords[i].token));
	}

	/* yeah, COMMIT should be a normal literal */
	g_assert_cmpint(TK_LITERAL, ==, sql_token_get_id("COMMIT"));
} END_TEST
/* @} */

int main(int argc, char **argv) {
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/tokenizer", test_tokenizer);
	g_test_add_func("/core/tokenizer_token2name", test_token2name);
	g_test_add_func("/core/tokenizer_keywork2token", test_keyword2token);
	g_test_add_func("/core/tokenizer_table_name_underscore", test_table_name_underscore);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
