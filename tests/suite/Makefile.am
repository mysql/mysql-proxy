SUBDIRS=base bugs

TESTS_ENVIRONMENT = \
	MYSQL_TEST_BIN="${MYSQL_TEST_BIN}" \
	LUA_USER_PATH="${top_srcdir}/lib/?.lua" \
	LUA_CPATH=".libs/?.so;${top_builddir}/lib/.libs/?.so" \
	top_builddir=${top_builddir} \
	builddir=${builddir} \
	srcdir=${srcdir}/ \
	lua ${srcdir}/run-tests.lua

TESTS=base

EXTRA_DIST=\
	   run-tests.lua \
	   lua-tests-wrapper.sh.in

