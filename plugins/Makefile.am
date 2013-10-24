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
SUBDIRS=\
	admin \
	proxy \
	replicant \
	debug 
# the cli plugin needs readline and we don't have it on all platforms
# cli

EXTRA_DIST=CMakeLists.txt

check-local:
	mkdir -p $(top_builddir)/plugins/; 
	for plugin in $(SUBDIRS); do \
		for i in `cat $(top_builddir)/plugins/$$plugin/.libs/lib$$plugin.la | grep '^dlname' | sed "s/.*=//;s/'//g"`; do \
			echo $$i; \
			ln -fs $(top_builddir)/plugins/$$plugin/.libs/$$i $(top_builddir)/plugins/$$i; \
		done; \
	done;

clean-local:
	## remove left-overs from the check-local hack in the plugings
