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
EXTRA_DIST=

example_scripts = \
	tutorial-basic.lua \
	tutorial-constants.lua \
	tutorial-inject.lua \
	tutorial-keepalive.lua \
	tutorial-monitor.lua \
	tutorial-packets.lua \
	tutorial-query-time.lua \
	tutorial-prep-stmts.lua \
	tutorial-resultset.lua \
	tutorial-rewrite.lua \
	tutorial-routing.lua \
	tutorial-scramble.lua \
	tutorial-states.lua \
	tutorial-tokenize.lua \
	tutorial-union.lua \
	tutorial-warnings.lua

if USE_WRAPPER_SCRIPT
dist_doc_DATA = ${example_scripts}
else
EXTRA_DIST += ${example_scripts}
endif

EXTRA_DIST+=CMakeLists.txt
