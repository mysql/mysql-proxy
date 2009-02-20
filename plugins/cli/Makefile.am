pkglib_LTLIBRARIES = libcli.la
libcli_la_LDFLAGS  = -export-dynamic -no-undefined -no-version -dynamic
libcli_la_SOURCES  = cli-plugin.c
libcli_la_LIBADD   = $(EVENT_LIBS) $(GLIB_LIBS) $(GMODULE_LIBS) $(top_builddir)/src/libmysql-proxy.la -lreadline
libcli_la_CPPFLAGS = $(MYSQL_CFLAGS) $(GLIB_CFLAGS) $(LUA_CFLAGS) $(GMODULE_CFLAGS) -I$(top_srcdir)/src/

# EXTRA_DIST=CMakeLists.txt

