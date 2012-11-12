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
## only run the tests if we have lua somewhere 
#
# for now this is unix specific as we use env-vars and shell-scripts
IF(LUA_EXECUTABLE AND UNIX )
	CONFIGURE_FILE(
		${CMAKE_CURRENT_SOURCE_DIR}/lua-tests-wrapper.sh.in 
		${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh
		@ONLY)
	ADD_TEST(lua-tests/base 
		sh -x ${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh 
		${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/run-tests.lua ${CMAKE_CURRENT_SOURCE_DIR}/base)
ENDIF(LUA_EXECUTABLE AND UNIX )
