if USE_CHECK
TESTS=check_sql_tokenizer
check_PROGRAMS=check_sql_tokenizer
check_sql_tokenizer_SOURCES  = check_sql_tokenizer.c $(top_srcdir)/src/sql-tokenizer.l
check_sql_tokenizer_CPPFLAGS = @CHECK_CFLAGS@ -I$(top_srcdir)/src/ $(GLIB_CFLAGS)
check_sql_tokenizer_LDADD    = @CHECK_LIBS@ $(GLIB_LIBS)
else
EXTRA_DIST=check_sql_tokenizer.c
endif
