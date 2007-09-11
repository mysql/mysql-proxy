sbin_PROGRAMS=mysql-proxy

mysql_proxy_SOURCES= \
	mysql-proxy.c \
	network-mysqld.c \
	network-mysqld-proto.c \
	network-mysqld-proxy.c \
	network-mysqld-server.c \
	network-mysqld-table.c \
	network-conn-pool.c  \
	network-socket.c \
	sql-tokenizer.l \
	glib-ext.c

noinst_HEADERS=\
	network-mysqld.h \
	network-mysqld-proto.h \
	network-conn-pool.h \
	network-socket.h \
	sys-pedantic.h \
	sql-tokenizer.h \
	glib-ext.h

mysql_proxy_CPPFLAGS = $(MYSQL_INCLUDE) $(GLIB_CFLAGS) $(LUA_CFLAGS)
mysql_proxy_LDADD = $(ENABLE_LINK_STATIC) $(EVENT_LIBS) $(GLIB_LIBS) $(LUA_LIBS) $(DISABLE_LINK_STATIC)
mysql_proxy_CFLAGS = -DLIBDIR="\"$(pkgdatadir)\""

## for now test-latency uses macros with var-args which isn't supported on
## some compilers. Include the source in tar.gz, but don't build it by default
# noinst_PROGRAMS=test-latency 
# test_latency_SOURCES=test-latency.c
# test_latency_CPPFLAGS= $(MYSQL_INCLUDE) $(GLIB_CFLAGS)
# test_latency_LDADD= $(MYSQL_LIBS) $(GLIB_LIBS)

EXTRA_DIST=CMakeLists.txt test-latency.c
