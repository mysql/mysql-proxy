%{
#include <stdio.h>
#include <string.h>

#include "sql-tokenizer.h"

#define YY_DECL int sql_tokenizer_internal(GPtrArray *tokens)

static void sql_token_append(GPtrArray *tokens, sql_token_id token_id, gchar *text);
static void sql_token_append_last_token(GPtrArray *tokens, sql_token_id token_id, gchar *text);
sql_token_id sql_token_get_id(const gchar *name);

const gchar *token_names[] = {
	"TK_UNKNOWN",
	"TK_LE",
	"TK_GE",
	"TK_LT",
	"TK_GT",
	"TK_EQ",
	"TK_NE",
	"TK_STRING",
	"TK_COMMENT",
	"TK_LITERAL",
	"TK_INTEGER",
	"TK_FLOAT",
	"TK_DOT",
	"TK_COMMA",
	"TK_ASSIGN",
	"TK_OBRACE",
	"TK_CBRACE",
	"TK_SEMICOLON",
	"TK_STAR",
	"TK_PLUS",
	"TK_MINUS",
	"TK_DIV",
	"TK_SQL_ACCESSIBLE",
	"TK_SQL_ACTION",
	"TK_SQL_ADD",
	"TK_SQL_ALL",
	"TK_SQL_ALTER",
	"TK_SQL_ANALYZE",
	"TK_SQL_AND",
	"TK_SQL_AS",
	"TK_SQL_ASC",
	"TK_SQL_ASENSITIVE",
	"TK_SQL_BEFORE",
	"TK_SQL_BETWEEN",
	"TK_SQL_BIGINT",
	"TK_SQL_BINARY",
	"TK_SQL_BIT",
	"TK_SQL_BLOB",
	"TK_SQL_BOTH",
	"TK_SQL_BY",
	"TK_SQL_CALL",
	"TK_SQL_CASCADE",
	"TK_SQL_CASE",
	"TK_SQL_CHANGE",
	"TK_SQL_CHAR",
	"TK_SQL_CHARACTER",
	"TK_SQL_CHECK",
	"TK_SQL_COLLATE",
	"TK_SQL_COLUMN",
	"TK_SQL_CONDITION",
	"TK_SQL_CONSTRAINT",
	"TK_SQL_CONTINUE",
	"TK_SQL_CONVERT",
	"TK_SQL_CREATE",
	"TK_SQL_CROSS",
	"TK_SQL_CURRENT_DATE",
	"TK_SQL_CURRENT_TIME",
	"TK_SQL_CURRENT_TIMESTAMP",
	"TK_SQL_CURRENT_USER",
	"TK_SQL_CURSOR",
	"TK_SQL_DATABASE",
	"TK_SQL_DATABASES",
	"TK_SQL_DATE",
	"TK_SQL_DAY_HOUR",
	"TK_SQL_DAY_MICROSECOND",
	"TK_SQL_DAY_MINUTE",
	"TK_SQL_DAY_SECOND",
	"TK_SQL_DEC",
	"TK_SQL_DECIMAL",
	"TK_SQL_DECLARE",
	"TK_SQL_DEFAULT",
	"TK_SQL_DELAYED",
	"TK_SQL_DELETE",
	"TK_SQL_DESC",
	"TK_SQL_DESCRIBE",
	"TK_SQL_DETERMINISTIC",
	"TK_SQL_DISTINCT",
	"TK_SQL_DISTINCTROW",
	"TK_SQL_DIV",
	"TK_SQL_DOUBLE",
	"TK_SQL_DROP",
	"TK_SQL_DUAL",
	"TK_SQL_EACH",
	"TK_SQL_ELSE",
	"TK_SQL_ELSEIF",
	"TK_SQL_ENCLOSED",
	"TK_SQL_ENUM",
	"TK_SQL_ESCAPED",
	"TK_SQL_EXISTS",
	"TK_SQL_EXIT",
	"TK_SQL_EXPLAIN",
	"TK_SQL_FALSE",
	"TK_SQL_FETCH",
	"TK_SQL_FLOAT",
	"TK_SQL_FLOAT4",
	"TK_SQL_FLOAT8",
	"TK_SQL_FOR",
	"TK_SQL_FORCE",
	"TK_SQL_FOREIGN",
	"TK_SQL_FROM",
	"TK_SQL_FULLTEXT",
	"TK_SQL_GRANT",
	"TK_SQL_GROUP",
	"TK_SQL_HAVING",
	"TK_SQL_HIGH_PRIORITY",
	"TK_SQL_HOUR_MICROSECOND",
	"TK_SQL_HOUR_MINUTE",
	"TK_SQL_HOUR_SECOND",
	"TK_SQL_IF",
	"TK_SQL_IGNORE",
	"TK_SQL_IN",
	"TK_SQL_INDEX",
	"TK_SQL_INFILE",
	"TK_SQL_INNER",
	"TK_SQL_INOUT",
	"TK_SQL_INSENSITIVE",
	"TK_SQL_INSERT",
	"TK_SQL_INT",
	"TK_SQL_INT1",
	"TK_SQL_INT2",
	"TK_SQL_INT3",
	"TK_SQL_INT4",
	"TK_SQL_INT8",
	"TK_SQL_INTEGER",
	"TK_SQL_INTERVAL",
	"TK_SQL_INTO",
	"TK_SQL_IS",
	"TK_SQL_ITERATE",
	"TK_SQL_JOIN",
	"TK_SQL_KEY",
	"TK_SQL_KEYS",
	"TK_SQL_KILL",
	"TK_SQL_LEADING",
	"TK_SQL_LEAVE",
	"TK_SQL_LEFT",
	"TK_SQL_LIKE",
	"TK_SQL_LIMIT",
	"TK_SQL_LINEAR",
	"TK_SQL_LINES",
	"TK_SQL_LOAD",
	"TK_SQL_LOCALTIME",
	"TK_SQL_LOCALTIMESTAMP",
	"TK_SQL_LOCK",
	"TK_SQL_LONG",
	"TK_SQL_LONGBLOB",
	"TK_SQL_LONGTEXT",
	"TK_SQL_LOOP",
	"TK_SQL_LOW_PRIORITY",
	"TK_SQL_MASTER_SSL_VERIFY_SERVER_CERT",
	"TK_SQL_MATCH",
	"TK_SQL_MEDIUMBLOB",
	"TK_SQL_MEDIUMINT",
	"TK_SQL_MEDIUMTEXT",
	"TK_SQL_MIDDLEINT",
	"TK_SQL_MINUTE_MICROSECOND",
	"TK_SQL_MINUTE_SECOND",
	"TK_SQL_MOD",
	"TK_SQL_MODIFIES",
	"TK_SQL_NATURAL",
	"TK_SQL_NO",
	"TK_SQL_NOT",
	"TK_SQL_NO_WRITE_TO_BINLOG",
	"TK_SQL_NULL",
	"TK_SQL_NUMERIC",
	"TK_SQL_ON",
	"TK_SQL_OPTIMIZE",
	"TK_SQL_OPTION",
	"TK_SQL_OPTIONALLY",
	"TK_SQL_OR",
	"TK_SQL_ORDER",
	"TK_SQL_OUT",
	"TK_SQL_OUTER",
	"TK_SQL_OUTFILE",
	"TK_SQL_PRECISION",
	"TK_SQL_PRIMARY",
	"TK_SQL_PROCEDURE",
	"TK_SQL_PURGE",
	"TK_SQL_RANGE",
	"TK_SQL_READ",
	"TK_SQL_READ_ONLY",
	"TK_SQL_READS",
	"TK_SQL_READ_WRITE",
	"TK_SQL_REAL",
	"TK_SQL_REFERENCES",
	"TK_SQL_REGEXP",
	"TK_SQL_RELEASE",
	"TK_SQL_RENAME",
	"TK_SQL_REPEAT",
	"TK_SQL_REPLACE",
	"TK_SQL_REQUIRE",
	"TK_SQL_RESTRICT",
	"TK_SQL_RETURN",
	"TK_SQL_REVOKE",
	"TK_SQL_RIGHT",
	"TK_SQL_RLIKE",
	"TK_SQL_SCHEMA",
	"TK_SQL_SCHEMAS",
	"TK_SQL_SECOND_MICROSECOND",
	"TK_SQL_SELECT",
	"TK_SQL_SENSITIVE",
	"TK_SQL_SEPARATOR",
	"TK_SQL_SET",
	"TK_SQL_SHOW",
	"TK_SQL_SMALLINT",
	"TK_SQL_SPATIAL",
	"TK_SQL_SPECIFIC",
	"TK_SQL_SQL",
	"TK_SQL_SQL_BIG_RESULT",
	"TK_SQL_SQL_CALC_FOUND_ROWS",
	"TK_SQL_SQLEXCEPTION",
	"TK_SQL_SQL_SMALL_RESULT",
	"TK_SQL_SQLSTATE",
	"TK_SQL_SQLWARNING",
	"TK_SQL_SSL",
	"TK_SQL_STARTING",
	"TK_SQL_STRAIGHT_JOIN",
	"TK_SQL_TABLE",
	"TK_SQL_TERMINATED",
	"TK_SQL_TEXT",
	"TK_SQL_THEN",
	"TK_SQL_TIME",
	"TK_SQL_TIMESTAMP",
	"TK_SQL_TINYBLOB",
	"TK_SQL_TINYINT",
	"TK_SQL_TINYTEXT",
	"TK_SQL_TO",
	"TK_SQL_TRAILING",
	"TK_SQL_TRIGGER",
	"TK_SQL_TRUE",
	"TK_SQL_UNDO",
	"TK_SQL_UNION",
	"TK_SQL_UNIQUE",
	"TK_SQL_UNLOCK",
	"TK_SQL_UNSIGNED",
	"TK_SQL_UPDATE",
	"TK_SQL_USAGE",
	"TK_SQL_USE",
	"TK_SQL_USING",
	"TK_SQL_UTC_DATE",
	"TK_SQL_UTC_TIME",
	"TK_SQL_UTC_TIMESTAMP",
	"TK_SQL_VALUES",
	"TK_SQL_VARBINARY",
	"TK_SQL_VARCHAR",
	"TK_SQL_VARCHARACTER",
	"TK_SQL_VARYING",
	"TK_SQL_WHEN",
	"TK_SQL_WHERE",
	"TK_SQL_WHILE",
	"TK_SQL_WITH",
	"TK_SQL_WRITE",
	"TK_SQL_X509",
	"TK_SQL_XOR",
	"TK_SQL_YEAR_MONTH",
	"TK_SQL_ZEROFILL",
	NULL
};

char quote_char = 0;
sql_token_id quote_token_id = TK_UNKNOWN;
%}

%option case-insensitive
%option noyywrap
%option never-interactive
%option 8bit
%option fast
%x COMMENT QUOTED
%%

	/** comments */
"/*"		sql_token_append(tokens, TK_COMMENT,    ""); BEGIN(COMMENT);
<COMMENT>[^*]*	sql_token_append_last_token(tokens, TK_COMMENT, yytext);
<COMMENT>"*"+[^*/]*	sql_token_append_last_token(tokens, TK_COMMENT, yytext);
<COMMENT>"*"+"/"	BEGIN(INITIAL);

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



	/** strings, quoting, literals */
[[:space:]]+	/** ignore WS */
[[:alpha:]@][[:alnum:]_@]*	sql_token_append(tokens, sql_token_get_id(yytext), yytext);

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

void sql_token_free(sql_token *token) {
	if (!token) return;

	g_string_free(token->text, TRUE);
	g_free(token);		
}

static void sql_token_append(GPtrArray *tokens, sql_token_id token_id, gchar *text) {
	sql_token *token;

	token = sql_token_new();
	token->token_id = token_id;
	g_string_assign(token->text, text);
	g_ptr_array_add(tokens, token);
}

static void sql_token_append_last_token(GPtrArray *tokens, sql_token_id token_id, gchar *text) {
	sql_token *token;

	g_assert(tokens->len > 0);

	token = tokens->pdata[tokens->len - 1];
	g_assert(token);
	g_assert(token->token_id == token_id);

	g_string_append(token->text, text);
}

const gchar *sql_token_get_name(sql_token_id token_id) {
	if (token_id >= TK_LAST_TOKEN) return NULL;

	if (sizeof(token_names)/sizeof(char *) != TK_LAST_TOKEN + 1) {
		g_error("sql_token_get_name() is out of sync [%ld != %d]", sizeof(token_names)/sizeof(char *), TK_LAST_TOKEN + 1);
	}

	return token_names[token_id];
}

/**
 * get the token_id for a literal 
 */
sql_token_id sql_token_get_id(const gchar *name) {
	gsize i;

	/** 
	 * iterating the list all the time is slooooow,
	 * build a hash at startup instead 
	 */
	for (i = 0; token_names[i]; i++) {
		if (0 != g_ascii_strncasecmp(token_names[i], "TK_SQL_", sizeof("TK_SQL_") - 1)) continue;

		if (0 == g_ascii_strcasecmp(token_names[i] + sizeof("TK_SQL_") - 1, name)) {
			return i;
		}
	}

	return TK_LITERAL;
}

int sql_tokenizer(GPtrArray *tokens, const gchar *str, gsize len) {
	YY_BUFFER_STATE state;
	int ret;

	state = yy_scan_bytes(str, len);
	ret = sql_tokenizer_internal(tokens);
	yy_delete_buffer(state);

	return ret;
}

#ifdef WITH_MAIN
int main() {
	GPtrArray *tokens = NULL;
	gsize i;

#define C(x) x, sizeof(x) - 1

	tokens = g_ptr_array_new();
	sql_tokenizer(tokens, C("SELEcT \"qq-end\"\"\", \"\"\"qq-start\", \"'\"`qq-mixed''\" FROM a AS `b`, `ABC``FOO` "));

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];

		printf("[%"G_GSIZE_FORMAT"] token=%s (%d) (%s)\n", i, sql_token_get_name(token->token_id), token->token_id, token->text->str);
	}

	for (i = 0; i < tokens->len; i++) {
		sql_token *token = tokens->pdata[i];

		sql_token_free(token);
	}
	g_ptr_array_free(tokens, TRUE);

	/* cleanup */
	return 0;
}
#endif
