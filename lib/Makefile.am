SUBDIRS = proxy 

dist_pkgdata_DATA = \
		active-queries.lua \
		active-transactions.lua \
		admin-sql.lua \
		analyze-query.lua \
		auditing.lua \
		commit-obfuscator.lua \
		commit-obfuscator.msc \
		histogram.lua \
		load-multi.lua \
		ro-balance.lua \
		ro-pooling.lua \
		rw-splitting.lua \
		xtab.lua

pkglib_LTLIBRARIES = lfs.la glib2.la chassis.la mysql.la lpeg.la posix.la

lfs_la_SOURCES  = lfs.c
## get libtool to build a shared-lib
lfs_la_CPPFLAGS = ${LUA_CFLAGS}
lfs_la_LDFLAGS  = $(AM_LDFLAGS) -module -avoid-version

posix_la_SOURCES  = posix.c
## get libtool to build a shared-lib
posix_la_CPPFLAGS = ${LUA_CFLAGS}
posix_la_LDFLAGS  = $(AM_LDFLAGS) -module -avoid-version

glib2_la_SOURCES  = glib2.c
## get libtool to build a shared-lib
glib2_la_CPPFLAGS = ${LUA_CFLAGS} ${GLIB_CFLAGS}
glib2_la_LDFLAGS  = $(AM_LDFLAGS) -module -avoid-version
glib2_la_LIBADD   = $(GLIB_LIBS)

chassis_la_SOURCES  = chassis.c
## get libtool to build a shared-lib
chassis_la_CPPFLAGS = ${LUA_CFLAGS} ${GLIB_CFLAGS} -I${top_srcdir}/src/
chassis_la_LDFLAGS  = $(AM_LDFLAGS) -module -avoid-version
chassis_la_LIBADD   = ${GLIB_LIBS} ${top_builddir}/src/libmysql-chassis.la

mysql_la_SOURCES  = \
	mysql-proto.c \
	sql-tokenizer.l \
	sql-tokenizer-lua.c 
## get libtool to build a shared-lib
mysql_la_CPPFLAGS = ${LUA_CFLAGS} ${GLIB_CFLAGS} -I${top_srcdir}/src/ ${MYSQL_CFLAGS} -I${top_builddir}/lib/
mysql_la_LDFLAGS  = $(AM_LDFLAGS) -module -avoid-version
mysql_la_LIBADD   = ${GLIB_LIBS} ${top_builddir}/src/libmysql-proxy.la

lpeg_la_SOURCES  = lpeg.c
lpeg_la_CPPFLAGS = ${LUA_CFLAGS}
lpeg_la_LDFLAGS  = $(AM_LDFLAGS) -module -avoid-version


noinst_HEADERS = \
	sql-tokenizer.h 

EXTRA_DIST = \
	glib2.def \
	lfs.def \
	chassis.def \
	lpeg.def \
	mysql.def \
	posix.def 

