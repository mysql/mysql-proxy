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
plugindir = ${pkglibdir}/plugins

plugin_LTLIBRARIES = libproxy.la
libproxy_la_LDFLAGS  = -export-dynamic -no-undefined -avoid-version -dynamic
libproxy_la_SOURCES  = proxy-plugin.c
libproxy_la_LIBADD   = $(EVENT_LIBS) $(GLIB_LIBS) $(GMODULE_LIBS) $(top_builddir)/src/libmysql-proxy.la
libproxy_la_CPPFLAGS = $(MYSQL_CFLAGS) $(GLIB_CFLAGS) $(LUA_CFLAGS) $(GMODULE_CFLAGS) -I$(top_srcdir)/src/
noinst_HEADERS = proxy-plugin.h

EXTRA_DIST=CMakeLists.txt

