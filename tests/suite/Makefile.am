#  $%BEGINLICENSE%$
#  Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.
# 
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; version 2 of the
#  License.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
#  GNU General Public License for more details.
# 
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
#  02110-1301  USA
# 
#  $%ENDLICENSE%$
SUBDIRS=base bugs

TESTS_ENVIRONMENT = \
	MYSQL_TEST_BIN="${MYSQL_TEST_BIN}" \
	LUA_USER_PATH="${top_srcdir}/lib/?.lua" \
	LUA_CPATH=".libs/?.@DYNLIB_LUA_SUFFIX@;${top_builddir}/lib/.libs/?.@DYNLIB_LUA_SUFFIX@" \
	@DYNLIB_PATH_VAR@="${top_builddir}/src/.libs/:${@DYNLIB_PATH_VAR@}" \
	MYSQL_PROXY_VERSION="${PACKAGE_VERSION}" \
	top_builddir=${top_builddir} \
	builddir=${builddir} \
	srcdir=${srcdir}/ \
	BUILD_OS=${build} \
	${builddir}/lua-runner ${srcdir}/run-tests.lua

TESTS=base

check_PROGRAMS=lua-runner

lua_runner_SOURCES=lua_runner.c
lua_runner_LDFLAGS=${LUA_LDFLAGS}
lua_runner_LDADD=${LUA_LIBS}
lua_runner_CFLAGS=${LUA_CFLAGS}


EXTRA_DIST=\
	run-tests.lua \
	lua-tests-wrapper.sh.in \
	CMakeLists.txt

clean-local:
	-rm `find . -type f -name "*.log"`
