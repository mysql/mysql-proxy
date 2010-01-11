%{
/*$%BEGINLICENSE%$
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

 $%ENDLICENSE%$*/


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
#include <stdlib.h>  /* for exit */
#endif

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
%option reentrant
%option extra-type="GPtrArray *"
%x COMMENT LINECOMMENT QUOTED
%%

	/** comments */
"--"\r?\n       comment_token_id = TK_COMMENT;       sql_token_append(yyget_extra(yyscanner), comment_token_id, "");
"/*"		comment_token_id = TK_COMMENT;       sql_token_append(yyget_extra(yyscanner), comment_token_id, ""); BEGIN(COMMENT);
"/*!"		comment_token_id = TK_COMMENT_MYSQL; sql_token_append(yyget_extra(yyscanner), comment_token_id, ""); BEGIN(COMMENT);
"--"[[:blank:]]		comment_token_id = TK_COMMENT; sql_token_append(yyget_extra(yyscanner), comment_token_id, ""); BEGIN(LINECOMMENT);
<COMMENT>[^*]*	sql_token_append_last_token(yyget_extra(yyscanner), comment_token_id, yyget_text(yyscanner));
<COMMENT>"*"+[^*/]*	sql_token_append_last_token(yyget_extra(yyscanner), comment_token_id, yyget_text(yyscanner));
<COMMENT>"*"+"/"	BEGIN(INITIAL);
<COMMENT><<EOF>>	BEGIN(INITIAL);
<LINECOMMENT>[^\n]* sql_token_append_last_token(yyget_extra(yyscanner), comment_token_id, yyget_text(yyscanner));
<LINECOMMENT>\r?\n	BEGIN(INITIAL);
<LINECOMMENT><<EOF>>	BEGIN(INITIAL);

	/** start of a quote string */
["'`]		{ BEGIN(QUOTED);  
		quote_char = *yyget_text(yyscanner); 
		switch (quote_char) { 
		case '\'': quote_token_id = TK_STRING; break; 
		case '"': quote_token_id = TK_STRING; break; 
		case '`': quote_token_id = TK_LITERAL; break; 
		} 
		sql_token_append(yyget_extra(yyscanner), quote_token_id, ""); }
<QUOTED>[^"'`\\]*	sql_token_append_last_token(yyget_extra(yyscanner), quote_token_id, yyget_text(yyscanner)); /** all non quote or esc chars are passed through */
<QUOTED>"\\".		sql_token_append_last_token(yyget_extra(yyscanner), quote_token_id, yyget_text(yyscanner)); /** add escaping */
<QUOTED>["'`]{2}	{ if (yyget_text(yyscanner)[0] == yyget_text(yyscanner)[1] && yyget_text(yyscanner)[1] == quote_char) { 
				sql_token_append_last_token(yyget_extra(yyscanner), quote_token_id, yyget_text(yyscanner) + 1);  /** doubling quotes */
			} else {
				/** the pick the first char and put the second back to parsing */
				yyless(1);
				sql_token_append_last_token(yyget_extra(yyscanner), quote_token_id, yyget_text(yyscanner));
			}
			}
<QUOTED>["'`]	if (*yyget_text(yyscanner) == quote_char) { BEGIN(INITIAL); } else { sql_token_append_last_token(yyget_extra(yyscanner), quote_token_id, yyget_text(yyscanner)); }
<QUOTED><<EOF>>	BEGIN(INITIAL);

	/** strings, quoting, literals */
[[:space:]]+	/** ignore WS */
	/** should be a literal */
[[:alpha:]_@][[:alnum:]_@]*	sql_token_append(yyget_extra(yyscanner), sql_token_get_id(yyget_text(yyscanner)), yyget_text(yyscanner));
[[:alpha:]_@][[:alnum:]_@]*\(	{ yyless(yyleng - 1); sql_token_append(yyget_extra(yyscanner), TK_FUNCTION, yyget_text(yyscanner)); }

[[:digit:]]+	sql_token_append(yyget_extra(yyscanner), TK_INTEGER, yyget_text(yyscanner));
[[:digit:]]*"."[[:digit:]]+	sql_token_append(yyget_extra(yyscanner), TK_FLOAT, yyget_text(yyscanner));
","		sql_token_append(yyget_extra(yyscanner), TK_COMMA, yyget_text(yyscanner));
"."		sql_token_append(yyget_extra(yyscanner), TK_DOT, yyget_text(yyscanner));

"<"		sql_token_append(yyget_extra(yyscanner), TK_LT, yyget_text(yyscanner));
">"		sql_token_append(yyget_extra(yyscanner), TK_GT, yyget_text(yyscanner));
"<="		sql_token_append(yyget_extra(yyscanner), TK_LE, yyget_text(yyscanner));
">="		sql_token_append(yyget_extra(yyscanner), TK_GE, yyget_text(yyscanner));
"="		sql_token_append(yyget_extra(yyscanner), TK_EQ, yyget_text(yyscanner));
"<>"		sql_token_append(yyget_extra(yyscanner), TK_NE, yyget_text(yyscanner));
"!="		sql_token_append(yyget_extra(yyscanner), TK_NE, yyget_text(yyscanner));

"("		sql_token_append(yyget_extra(yyscanner), TK_OBRACE, yyget_text(yyscanner));
")"		sql_token_append(yyget_extra(yyscanner), TK_CBRACE, yyget_text(yyscanner));
";"		sql_token_append(yyget_extra(yyscanner), TK_SEMICOLON, yyget_text(yyscanner));
":="		sql_token_append(yyget_extra(yyscanner), TK_ASSIGN, yyget_text(yyscanner));

"*"		sql_token_append(yyget_extra(yyscanner), TK_STAR, yyget_text(yyscanner));
"+"		sql_token_append(yyget_extra(yyscanner), TK_PLUS, yyget_text(yyscanner));
"/"		sql_token_append(yyget_extra(yyscanner), TK_DIV, yyget_text(yyscanner));
"-"		sql_token_append(yyget_extra(yyscanner), TK_MINUS, yyget_text(yyscanner));

"&"		sql_token_append(yyget_extra(yyscanner), TK_BITWISE_AND, yyget_text(yyscanner));
"&&"		sql_token_append(yyget_extra(yyscanner), TK_LOGICAL_AND, yyget_text(yyscanner));
"|"		sql_token_append(yyget_extra(yyscanner), TK_BITWISE_OR, yyget_text(yyscanner));
"||"		sql_token_append(yyget_extra(yyscanner), TK_LOGICAL_OR, yyget_text(yyscanner));

"^"		sql_token_append(yyget_extra(yyscanner), TK_BITWISE_XOR, yyget_text(yyscanner));

	/** the default rule */
.		sql_token_append(yyget_extra(yyscanner), TK_UNKNOWN, yyget_text(yyscanner));

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
	yyscan_t scanner;
	int ret;

	yylex_init_extra(tokens, &scanner);
	state = yy_scan_bytes(str, len, scanner);
	ret = yylex(scanner);
	yy_delete_buffer(state, scanner);
	yylex_destroy(scanner);

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

