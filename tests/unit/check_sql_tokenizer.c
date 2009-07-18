/* $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

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


/**
 * @test check if single line comments are recognized properly
 */
START_TEST(test_simple_dashdashcomment) {
	gsize i;
	GPtrArray *tokens = NULL;
	
	tokens = sql_tokens_new();
	
	sql_tokenizer(tokens, C("-- comment"));

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];
		
#define T(t_id, t_text) \
g_assert_cmpint(token->token_id, ==, t_id); \
g_assert_cmpstr(token->text->str, ==, t_text); 

		switch (i) {
		case 0: T(TK_COMMENT, "comment"); break;
		default: g_assert(FALSE); break;
#undef T
		}
	}

	sql_tokens_free(tokens);

} END_TEST

/**
 * @test check if single line comments are recognized properly
 */
START_TEST(test_dashdashcomment) {
	gsize i;
	GPtrArray *tokens = NULL;
	
	tokens = sql_tokens_new();
	
	sql_tokenizer(tokens, C("--  comment\nSELECT 1 FROM dual"));
	
	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];
		
#define T(t_id, t_text) \
g_assert_cmpint(token->token_id, ==, t_id); \
g_assert_cmpstr(token->text->str, ==, t_text); 
		
		switch (i) {
			case 0: T(TK_COMMENT, " comment"); break;	/* note the leading whitespace here! */
			case 1: T(TK_SQL_SELECT, "SELECT"); break;
			case 2: T(TK_INTEGER, "1"); break;
			case 3: T(TK_SQL_FROM, "FROM"); break;
			case 4: T(TK_SQL_DUAL, "dual"); break;
			default: g_assert(FALSE); break;
#undef T
		}
	}
	
	sql_tokens_free(tokens);
	
} END_TEST

/**
 * @test check that '--1' will not start a comment
 */
START_TEST(test_doubleminus) {
	gsize i;
	GPtrArray *tokens = NULL;
	
	tokens = sql_tokens_new();
	
	sql_tokenizer(tokens, C("SELECT 1--1 FROM DUAL"));

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];

#define T(t_id, t_text) \
g_assert_cmpint(token->token_id, ==, t_id); \
g_assert_cmpstr(token->text->str, ==, t_text); 
	
		switch (i) {
			case 0: T(TK_SQL_SELECT, "SELECT"); break;
			case 1: T(TK_INTEGER, "1"); break;
			case 2: T(TK_MINUS, "-"); break;
			case 3: T(TK_MINUS, "-"); break;
			case 4: T(TK_INTEGER, "1"); break;
			case 5: T(TK_SQL_FROM, "FROM"); break;
			case 6: T(TK_SQL_DUAL, "DUAL"); break;
			default: g_assert(FALSE); break;
#undef T
		}
	}	
	
	sql_tokens_free(tokens);
} END_TEST

/**
 * @test First test for bug 36506, where EOF encountered while the tokenizer is in a start start
 *       corrupts its internal state resulting in failure to correctly tokenize the subsequent query.
 */
START_TEST(test_startstate_reset_quoted) {
	gsize i;
	GPtrArray *tokens = NULL;
	
	tokens = sql_tokens_new();
	/* EOF encountered while sql-tokenizer is in QUOTED start state */
	sql_tokenizer(tokens, C("SELECT \"foo"));
	sql_tokens_free(tokens);

	tokens = sql_tokens_new();
	/* valid query, fails with an assertion when bug 36506 is unfixed */
	sql_tokenizer(tokens, C("SELECT \"foo\""));

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];
		
#define T(t_id, t_text) \
g_assert_cmpint(token->token_id, ==, t_id); \
g_assert_cmpstr(token->text->str, ==, t_text); 
		
		switch (i) {
			case 0: T(TK_SQL_SELECT, "SELECT"); break;
			case 1: T(TK_STRING, "foo"); break;
			default: g_assert(FALSE); break;
#undef T
		}
	}	
	
	sql_tokens_free(tokens);
	
} END_TEST

/**
 * @test Second test for bug 36506, where EOF encountered while the tokenizer is in a start start
 *       corrupts its internal state resulting in failure to correctly tokenize the subsequent query.
 */
START_TEST(test_startstate_reset_comment) {
	gsize i;
	GPtrArray *tokens = NULL;
	
	/* test for C-style comments */
	
	tokens = sql_tokens_new();
	/* EOF encountered while sql-tokenizer is in COMMENT start state */
	sql_tokenizer(tokens, C("SELECT /* foo"));
	sql_tokens_free(tokens);
	
	tokens = sql_tokens_new();
	/* valid query, fails with an assertion when bug 36506 is unfixed */
	sql_tokenizer(tokens, C("SELECT /*foo*/ 1"));
	
	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];
		
#define T(t_id, t_text) \
g_assert_cmpint(token->token_id, ==, t_id); \
g_assert_cmpstr(token->text->str, ==, t_text); 
		
		switch (i) {
			case 0: T(TK_SQL_SELECT, "SELECT"); break;
			case 1: T(TK_COMMENT, "foo"); break;
			case 2: T(TK_INTEGER, "1"); break;
			default: g_assert(FALSE); break;
#undef T
		}
	}	
	
	sql_tokens_free(tokens);

	/* test for line comments */
	
	tokens = sql_tokens_new();
	/* EOF encountered while sql-tokenizer is in LINECOMMENT start state */
	sql_tokenizer(tokens, C("SELECT -- foo"));
	sql_tokens_free(tokens);
	
	tokens = sql_tokens_new();
	/* valid query, fails with an assertion when bug 36506 is unfixed */
	sql_tokenizer(tokens, C("SELECT -- foo\n1"));
	
	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];
		
#define T(t_id, t_text) \
g_assert_cmpint(token->token_id, ==, t_id); \
g_assert_cmpstr(token->text->str, ==, t_text); 
		
		switch (i) {
			case 0: T(TK_SQL_SELECT, "SELECT"); break;
			case 1: T(TK_COMMENT, "foo"); break;
			case 2: T(TK_INTEGER, "1"); break;
			default: g_assert(FALSE); break;
#undef T
		}
	}	
	
	sql_tokens_free(tokens);
	
} END_TEST
/* @} */

void test_g_istr_hash() {
	g_assert_cmpint(g_istr_hash("foo"), ==, g_istr_hash("foo"));
	g_assert_cmpint(g_istr_hash("foo"), !=, g_istr_hash("boo"));
}

int main(int argc, char **argv) {
	g_test_init(&argc, &argv, NULL);
	g_test_bug_base("http://bugs.mysql.com/");

	g_test_add_func("/core/hash_str_i", test_g_istr_hash);

	g_test_add_func("/core/tokenizer", test_tokenizer);
	g_test_add_func("/core/tokenizer_token2name", test_token2name);
	g_test_add_func("/core/tokenizer_keywork2token", test_keyword2token);
	g_test_add_func("/core/tokenizer_table_name_underscore", test_table_name_underscore);
	g_test_add_func("/core/tokenizer_simple_dashdashcomment", test_simple_dashdashcomment);
	g_test_add_func("/core/tokenizer_dashdashcomment", test_dashdashcomment);
	g_test_add_func("/core/tokenizer_doubleminus", test_doubleminus);
	g_test_add_func("/core/tokenizer_startstate_reset_quoted", test_startstate_reset_quoted);
	g_test_add_func("/core/tokenizer_startstate_reset_comment", test_startstate_reset_comment);

	return g_test_run();
}
#else
int main() {
	return 77;
}
#endif
