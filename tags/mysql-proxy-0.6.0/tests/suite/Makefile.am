SUBDIRS=base bugs

TESTS_ENVIRONMENT = \
	MYSQL_TEST_BIN="${MYSQL_TEST_BIN}" \
	LUA_USER_PATH="${top_srcdir}/lib/?.lua" \
	LUA_CPATH=".libs/?.so" \
	builddir=${top_builddir} \
	lua ${srcdir}/run-tests.lua

TESTS=base

check_LTLIBRARIES = lfs.la

lfs_la_SOURCES  = lfs.c
## get libtool to build a shared-lib
lfs_la_CPPFLAGS = ${LUA_CFLAGS}
lfs_la_LDFLAGS  = -rpath /usr/lib $(AM_LDFLAGS) -module -avoid-version -no-undefined
lfs_la_LIBADD   = ${LUA_LIBS}

EXTRA_DIST=run-tests.lua 

