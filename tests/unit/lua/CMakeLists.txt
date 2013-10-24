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
IF(LUA_EXECUTABLE AND NOT WIN32)
	CONFIGURE_FILE(
		${CMAKE_CURRENT_SOURCE_DIR}/lua-tests-wrapper.sh.in 
		${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh
		@ONLY)
	ADD_TEST(lua-unit-tests/chassis 
		sh ${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh 
		${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/chassis.lua)
	ADD_TEST(lua-unit-tests/proxy-test 
		sh ${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh 
		${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/proxy-test.lua)
	ADD_TEST(lua-unit-tests/proxy-auto-config 
		sh ${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh 
		${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/proxy-auto-config.lua)
	ADD_TEST(lua-unit-tests/active-queries 
		sh ${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh 
		${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/active-queries.lua)
	ADD_TEST(lua-unit-tests/proxy-commands 
		sh ${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh 
		${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/proxy-commands.lua)
	ADD_TEST(lua-unit-tests/proxy-tokenizer 
		sh ${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh 
		${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/proxy-tokenizer.lua)
	ADD_TEST(lua-unit-tests/proxy-parser 
		sh ${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh 
		${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/proxy-parser.lua)
	ADD_TEST(lua-unit-tests/mysql-proto 
		sh ${CMAKE_CURRENT_BINARY_DIR}/lua-tests-wrapper.sh 
		${LUA_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/mysql-proto.lua)
ENDIF(LUA_EXECUTABLE AND NOT WIN32)
