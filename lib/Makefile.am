#  $%BEGINLICENSE%$
#  Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.
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
luaextdir = ${pkglibdir}/lua

SUBDIRS = proxy 

## those are in the end examples or proof-of-concepts
example_scripts = \
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
EXTRA_DIST = 

if USE_WRAPPER_SCRIPT
## only install them if we are self-contained
## otherwise let the packager decide where to put them
dist_doc_DATA = ${example_scripts}
else
EXTRA_DIST += ${example_scripts}
endif

dist_luaext_DATA = \
	admin.lua

luaext_LTLIBRARIES = lfs.la glib2.la chassis.la mysql.la lpeg.la posix.la

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
	mysql-password.c \
	sql-tokenizer.l \
	sql-tokenizer-tokens.c \
	sql-tokenizer-keywords.c \
	sql-tokenizer-lua.c 
## get libtool to build a shared-lib
mysql_la_CPPFLAGS = ${LUA_CFLAGS} ${GLIB_CFLAGS} -I${top_srcdir}/src/ ${MYSQL_CFLAGS} -I${top_builddir}/lib/
mysql_la_LDFLAGS  = $(AM_LDFLAGS) -module -avoid-version
mysql_la_LIBADD   = ${GLIB_LIBS} ${top_builddir}/src/libmysql-proxy.la

lpeg_la_SOURCES  = lpeg.c
lpeg_la_CPPFLAGS = ${LUA_CFLAGS}
lpeg_la_LDFLAGS  = $(AM_LDFLAGS) -module -avoid-version


noinst_HEADERS = \
	sql-tokenizer.h  \
	sql-tokenizer-keywords.h 

EXTRA_DIST += \
	glib2.def \
	lfs.def \
	chassis.def \
	lpeg.def \
	mysql.def \
	posix.def \
	CMakeLists.txt

DISTCLEANFILES = \
	sql-tokenizer.c \
	sql-tokenizer-keywords.c

noinst_PROGRAMS=sql-tokenizer-gen

sql_tokenizer_gen_SOURCES=\
	sql-tokenizer-gen.c \
	sql-tokenizer-tokens.c

sql_tokenizer_gen_CPPFLAGS=${GLIB_CFLAGS} -I${srcdir}
sql_tokenizer_gen_LDADD=${GLIB_LIBS}

sql-tokenizer.c: sql-tokenizer-keywords.c

sql-tokenizer-keywords.c: sql-tokenizer-gen
	${builddir}/sql-tokenizer-gen > ${builddir}/sql-tokenizer-keywords.c


