%{
/* $%BEGINLICENSE%$
 Copyright (c) 2007, 2010, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; version 2 of the
 License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA

 $%ENDLICENSE%$ */


#include <string.h>

#include "sql-tokenizer.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib-ext.h>

#ifdef WIN32
#include <io.h>  /* for read */
#endif
#include <stdlib.h>

#define YY_DECL int sql_tokenizer_internal(GPtrArray *tokens)

static void sql_token_append(GPtrArray *tokens, sql_token_id token_id, gchar *text);
static void sql_token_append_last_token(GPtrArray *tokens, sql_token_id token_id, gchar *text);
sql_token_id sql_token_get_id(const gchar *name);

#include "sql-tokenizer-keywords.h" /* generated, brings in sql_keywords */

char quote_char = 0;
sql_token_id quote_token_id = TK_UNKNOWN;
sql_token_id comment_token_id = TK_UNKNOWN;
%}

%option case-insensitive
%option noyywrap
%option never-interactive
%option 8bit
%option fast
%x COMMENT LINECOMMENT QUOTED
%%

	/** comments */
"--"\r?\n       comment_token_id = TK_COMMENT;       sql_token_append(tokens, comment_token_id, "");
"/*"		comment_token_id = TK_COMMENT;       sql_token_append(tokens, comment_token_id, ""); BEGIN(COMMENT);
"/*!"		comment_token_id = TK_COMMENT_MYSQL; sql_token_append(tokens, comment_token_id, ""); BEGIN(COMMENT);
"--"[[:blank:]]		comment_token_id = TK_COMMENT; sql_token_append(tokens, comment_token_id, ""); BEGIN(LINECOMMENT);
<COMMENT>[^*]*	sql_token_append_last_token(tokens, comment_token_id, yytext);
<COMMENT>"*"+[^*/]*	sql_token_append_last_token(tokens, comment_token_id, yytext);
<COMMENT>"*"+"/"	BEGIN(INITIAL);
<COMMENT><<EOF>>	BEGIN(INITIAL);
<LINECOMMENT>[^\n]* sql_token_append_last_token(tokens, comment_token_id, yytext);
<LINECOMMENT>\r?\n	BEGIN(INITIAL);
<LINECOMMENT><<EOF>>	BEGIN(INITIAL);

	/** start of a quote string */
["'`]		{ BEGIN(QUOTED);  
		quote_char = *yytext; 
		switch (quote_char) { 
		case '\'': quote_token_id = TK_STRING; break; 
		case '"': quote_token_id = TK_STRING; break; 
		case '`': quote_token_id = TK_LITERAL; break; 
		} 
		sql_token_append(tokens, quote_token_id, ""); }
<QUOTED>[^"'`\\]*	sql_token_append_last_token(tokens, quote_token_id, yytext); /** all non quote or esc chars are passed through */
<QUOTED>"\\".		sql_token_append_last_token(tokens, quote_token_id, yytext); /** add escaping */
<QUOTED>["'`]{2}	{ if (yytext[0] == yytext[1] && yytext[1] == quote_char) { 
				sql_token_append_last_token(tokens, quote_token_id, yytext + 1);  /** doubling quotes */
			} else {
				/** the pick the first char and put the second back to parsing */
				yyless(1);
				sql_token_append_last_token(tokens, quote_token_id, yytext);
			}
			}
<QUOTED>["'`]	if (*yytext == quote_char) { BEGIN(INITIAL); } else { sql_token_append_last_token(tokens, quote_token_id, yytext); }
<QUOTED><<EOF>>	BEGIN(INITIAL);

	/** strings, quoting, literals */
[[:space:]]+	/** ignore WS */
	/* literals (and function names) are
	 * - all alpha-nums that are not digits-only
	 *   1tbl, tbl1, t1t, 1t1 are all literals
	 *   1 is a number
	 */
[[:digit:]]*[[:alpha:]_@][[:alnum:]_@]*	sql_token_append(tokens, sql_token_get_id(yytext), yytext);
[[:digit:]]*[[:alpha:]_@][[:alnum:]_@]*\(	{ yyless(yyleng - 1); sql_token_append(tokens, TK_FUNCTION, yytext); }

[[:digit:]]+	sql_token_append(tokens, TK_INTEGER, yytext);
[[:digit:]]*"."[[:digit:]]+	sql_token_append(tokens, TK_FLOAT, yytext);
","		sql_token_append(tokens, TK_COMMA, yytext);
"."		sql_token_append(tokens, TK_DOT, yytext);

"<"		sql_token_append(tokens, TK_LT, yytext);
">"		sql_token_append(tokens, TK_GT, yytext);
"<="		sql_token_append(tokens, TK_LE, yytext);
">="		sql_token_append(tokens, TK_GE, yytext);
"="		sql_token_append(tokens, TK_EQ, yytext);
"<>"		sql_token_append(tokens, TK_NE, yytext);
"!="		sql_token_append(tokens, TK_NE, yytext);

"("		sql_token_append(tokens, TK_OBRACE, yytext);
")"		sql_token_append(tokens, TK_CBRACE, yytext);
";"		sql_token_append(tokens, TK_SEMICOLON, yytext);
":="		sql_token_append(tokens, TK_ASSIGN, yytext);

"*"		sql_token_append(tokens, TK_STAR, yytext);
"+"		sql_token_append(tokens, TK_PLUS, yytext);
"/"		sql_token_append(tokens, TK_DIV, yytext);
"-"		sql_token_append(tokens, TK_MINUS, yytext);

"&"		sql_token_append(tokens, TK_BITWISE_AND, yytext);
"&&"		sql_token_append(tokens, TK_LOGICAL_AND, yytext);
"|"		sql_token_append(tokens, TK_BITWISE_OR, yytext);
"||"		sql_token_append(tokens, TK_LOGICAL_OR, yytext);

"^"		sql_token_append(tokens, TK_BITWISE_XOR, yytext);

	/** the default rule */
.		sql_token_append(tokens, TK_UNKNOWN, yytext);

%%
sql_token *sql_token_new(void) {
	sql_token *tk;

	tk = g_new0(sql_token, 1);
	tk->text = g_string_new(NULL);
	tk->token_id = TK_UNKNOWN;

	return tk;
}

/**
 * free a sql-token
 */
void sql_token_free(sql_token *token) {
	if (!token) return;

	g_string_free(token->text, TRUE);
	g_free(token);		
}

/**
 * append a token to the token-list
 */
static void sql_token_append(GPtrArray *tokens, sql_token_id token_id, gchar *text) {
	sql_token *token;

	token = sql_token_new();
	token->token_id = token_id;
	g_string_assign(token->text, text);
	g_ptr_array_add(tokens, token);
}

/**
 * append text to the last token in the token-list
 */
static void sql_token_append_last_token(GPtrArray *tokens, sql_token_id token_id, gchar *text) {
	sql_token *token;

	g_assert(tokens->len > 0);

	token = tokens->pdata[tokens->len - 1];
	g_assert(token);
	g_assert(token->token_id == token_id);

	g_string_append(token->text, text);
}

static int sql_token_cmp(const void *_a, const void *_b) {
	int i               = *(int *)_b;
	const char *name    = _a;
	const char *keyword; 

	keyword = sql_token_get_name(i) + sizeof("TK_SQL_") - 1;
	g_assert(keyword); /* if this isn't true, we have a internal problem */
	
	return g_ascii_strcasecmp(name, keyword);
}

/**
 * get the token_id for a literal 
 */
sql_token_id sql_token_get_id(const gchar *name) {
	gint *i;

	/* do a binary search on the sql_keywords */

	i = bsearch(name,
		sql_keywords_get(),
		sql_keywords_get_count(),
		sizeof(int),
		sql_token_cmp);
	
	return i ? *i : TK_LITERAL; /* if we didn't find it, it is literal */
}

/**
 * scan a string into SQL tokens
 */
int sql_tokenizer(GPtrArray *tokens, const gchar *str, gsize len) {
	YY_BUFFER_STATE state;
	int ret;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	g_static_mutex_lock(&mutex);
	state = yy_scan_bytes(str, len);
	ret = sql_tokenizer_internal(tokens);
	yy_delete_buffer(state);
	g_static_mutex_unlock(&mutex);

	return ret;
}

GPtrArray *sql_tokens_new(void) {
	return g_ptr_array_new();
}

void sql_tokens_free(GPtrArray *tokens) {
	gsize i;
	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];

		sql_token_free(token);
	}
	g_ptr_array_free(tokens, TRUE);
}

