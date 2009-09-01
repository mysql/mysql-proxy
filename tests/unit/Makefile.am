SUBDIRS=lua

TESTS=\
	check_sql_tokenizer \
	check_mysqld_proto \
	check_plugin \
	check_loadscript \
	check_chassis_log \
	t_network_socket \
	t_network_address \
	t_network_backend \
	t_network_injection \
	t_network_mysqld_packet \
	t_network_mysqld_masterinfo \
	t_chassis_timings \
	check_chassis_path

check_PROGRAMS=$(TESTS)
check_sql_tokenizer_SOURCES  = check_sql_tokenizer.c $(top_srcdir)/lib/sql-tokenizer.l
check_sql_tokenizer_CPPFLAGS = -I$(top_srcdir)/lib/ $(GLIB_CFLAGS)
check_sql_tokenizer_LDADD    = $(GLIB_LIBS)

DISTCLEANFILES = \
	sql-tokenizer.c

check_mysqld_proto_SOURCES  = \
	check_mysqld_proto.c \
	$(top_srcdir)/src/network-mysqld-binlog.c \
	$(top_srcdir)/src/network-mysqld-proto.c \
	$(top_srcdir)/src/glib-ext.c

check_mysqld_proto_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_CFLAGS)
check_mysqld_proto_LDADD    = $(GLIB_LIBS)

t_network_mysqld_packet_SOURCES  = \
	t_network_mysqld_packet.c \
	$(top_srcdir)/src/network-mysqld-packet.c \
	$(top_srcdir)/src/network-mysqld-proto.c \
	$(top_srcdir)/src/network-socket.c \
	$(top_srcdir)/src/network-address.c \
	$(top_srcdir)/src/glib-ext.c

t_network_mysqld_packet_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(LUA_CFLAGS)
t_network_mysqld_packet_LDADD    = $(GLIB_LIBS) $(LUA_LIBS) $(EVENT_LIBS)

t_chassis_timings_SOURCES  = \
	t_chassis_timings.c \
	$(top_srcdir)/src/chassis-timings.c \
	$(top_srcdir)/src/my_rdtsc.c 

t_chassis_timings_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) 
t_chassis_timings_LDADD    = $(GLIB_LIBS)



check_plugin_SOURCES  = check_plugin.c $(top_srcdir)/src/chassis-plugin.c
check_plugin_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(GMODULE_CFLAGS)
check_plugin_LDADD    = $(GLIB_LIBS) $(GMODULE_LIBS)

check_chassis_log_SOURCES  = check_chassis_log.c $(top_srcdir)/src/chassis-log.c
check_chassis_log_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(GMODULE_CFLAGS)
check_chassis_log_LDADD    = $(GLIB_LIBS) $(GMODULE_LIBS)

check_loadscript_SOURCES  = check_loadscript.c $(top_srcdir)/src/lua-scope.c $(top_srcdir)/src/lua-load-factory.c $(top_srcdir)/src/chassis-stats.c
check_loadscript_CPPFLAGS = -I$(top_srcdir)/src/ $(LUA_CFLAGS) $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(GMODULE_CFLAGS)
check_loadscript_LDADD    = $(GLIB_LIBS) $(GMODULE_LIBS) $(GTHREAD_LIBS) $(LUA_LIBS)

t_network_socket_SOURCES  = \
	t_network_socket.c \
	$(top_srcdir)/src/glib-ext.c \
	$(top_srcdir)/src/network-mysqld-proto.c \
	$(top_srcdir)/src/network-mysqld-packet.c \
	$(top_srcdir)/src/network-address.c \
	$(top_srcdir)/src/network-socket.c

t_network_socket_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(GMODULE_CFLAGS) $(EVENT_CFLAGS) $(LUA_CFLAGS)
t_network_socket_LDADD    = $(GLIB_LIBS) $(GMODULE_LIBS) $(GTHREAD_LIBS) $(EVENT_LIBS) $(LUA_LIBS)

t_network_address_SOURCES  = \
	t_network_address.c \
	$(top_srcdir)/src/glib-ext.c \
	$(top_srcdir)/src/network-address.c 

t_network_address_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(GMODULE_CFLAGS) $(EVENT_CFLAGS) $(LUA_CFLAGS)
t_network_address_LDADD    = $(GLIB_LIBS) $(GMODULE_LIBS) $(GTHREAD_LIBS) $(EVENT_LIBS) $(LUA_LIBS)


check_chassis_path_SOURCES = \
	check_chassis_path.c \
	$(top_srcdir)/src/chassis-log.c \
	$(top_srcdir)/src/chassis-mainloop.c \
	$(top_srcdir)/src/chassis-event-thread.c \
	$(top_srcdir)/src/chassis-plugin.c \
	$(top_srcdir)/src/chassis-stats.c
check_chassis_path_CPPFLAGS = -I$(top_srcdir)/src $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(GMODULE_CFLAGS)
check_chassis_path_LDADD    = $(GLIB_LIBS) $(GMODULE_LIBS) $(EVENT_LIBS) $(GTHREAD_LIBS)

t_network_backend_SOURCES  = \
	t_network_backend.c \
	$(top_srcdir)/src/glib-ext.c \
	$(top_srcdir)/src/network-backend.c \
	$(top_srcdir)/src/network-mysqld-proto.c \
	$(top_srcdir)/src/network-mysqld-packet.c \
	$(top_srcdir)/src/network-conn-pool.c \
	$(top_srcdir)/src/network-address.c \
	$(top_srcdir)/src/network-socket.c

t_network_backend_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(GMODULE_CFLAGS) $(EVENT_CFLAGS) $(LUA_CFLAGS)
t_network_backend_LDADD    = $(GLIB_LIBS) $(GMODULE_LIBS) $(GTHREAD_LIBS) $(EVENT_LIBS) $(LUA_LIBS)

t_network_mysqld_masterinfo_SOURCES  = \
	t_network_mysqld_masterinfo.c \
	$(top_srcdir)/src/glib-ext.c \
	$(top_srcdir)/src/network-mysqld-proto.c \
	$(top_srcdir)/src/network-mysqld-masterinfo.c 

t_network_mysqld_masterinfo_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(GMODULE_CFLAGS) $(EVENT_CFLAGS)
t_network_mysqld_masterinfo_LDADD    = $(GLIB_LIBS) $(GMODULE_LIBS) $(GTHREAD_LIBS) $(EVENT_LIBS)

t_network_injection_SOURCES  = \
	t_network_injection.c \
	$(top_srcdir)/src/glib-ext.c \
	$(top_srcdir)/src/network-mysqld-proto.c \
	$(top_srcdir)/src/network-mysqld-packet.c \
	$(top_srcdir)/src/network-injection.c 

t_network_injection_CPPFLAGS = -I$(top_srcdir)/src/ $(GLIB_CFLAGS) $(MYSQL_CFLAGS) $(GMODULE_CFLAGS) $(EVENT_CFLAGS) $(LUA_CFLAGS)
t_network_injection_LDADD    = $(GLIB_LIBS) $(GMODULE_LIBS) $(GTHREAD_LIBS) $(EVENT_LIBS) $(LUA_LIBS)


EXTRA_DIST = CMakeLists.txt

## remove the test-output
distclean-local:
	rm -f ${builddir}/test-*.txt
