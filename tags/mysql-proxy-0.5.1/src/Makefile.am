sbin_PROGRAMS=mysql-proxy

mysql_proxy_SOURCES= \
	mysql-proxy.c \
	network-mysqld.c \
	network-mysqld-proto.c \
	network-mysqld-proxy.c \
	network-mysqld-server.c \
	network-mysqld-table.c

noinst_HEADERS=\
	network-mysqld.h \
	network-mysqld-proto.h \
	sys-pedantic.h

mysql_proxy_CPPFLAGS = $(MYSQL_INCLUDE) $(GLIB_CFLAGS) $(LUA_CFLAGS)
mysql_proxy_LDADD = $(ENABLE_LINK_STATIC) $(EVENT_LIBS) $(GLIB_LIBS) $(LUA_LIBS) $(DISABLE_LINK_STATIC)
