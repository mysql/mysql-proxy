if USE_CHECK
TESTS=check_sql_tokenizer check_mysqld_proto

check_PROGRAMS=check_sql_tokenizer check_mysqld_proto
check_sql_tokenizer_SOURCES  = check_sql_tokenizer.c $(top_srcdir)/src/sql-tokenizer.l
check_sql_tokenizer_CPPFLAGS = @CHECK_CFLAGS@ -I$(top_srcdir)/src/ $(GLIB_CFLAGS)
check_sql_tokenizer_LDADD    = @CHECK_LIBS@ $(GLIB_LIBS)

check_mysqld_proto_SOURCES  = check_mysqld_proto.c $(top_srcdir)/src/network-mysqld-proto.c
check_mysqld_proto_CPPFLAGS = @CHECK_CFLAGS@ -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_INCLUDE)
check_mysqld_proto_LDADD    = @CHECK_LIBS@ $(GLIB_LIBS)

else
EXTRA_DIST=check_sql_tokenizer.c check_mysql_proto.c
endif
