INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}) # for config.h
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

INCLUDE_DIRECTORIES(${GLIB_INCLUDE_DIRS})
LINK_DIRECTORIES(${GLIB_LIBRARY_DIRS})

INCLUDE_DIRECTORIES(${MYSQL_INCLUDE_DIRS})
LINK_DIRECTORIES(${MYSQL_LIBRARY_DIRS})

INCLUDE_DIRECTORIES(${LUA_INCLUDE_DIRS})
LINK_DIRECTORIES(${LUA_LIBRARY_DIRS})

INCLUDE_DIRECTORIES(${EVENT_INCLUDE_DIRS})
LINK_DIRECTORIES(${EVENT_LIBRARY_DIRS})

STRING(REPLACE "." "" SHARED_LIBRARY_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
ADD_DEFINITIONS(-DSHARED_LIBRARY_SUFFIX="${SHARED_LIBRARY_SUFFIX}")

SET(chassis_sources 
	glib-ext.c
	lua-load-factory.c
	lua-scope.c
	chassis-plugin.c
	chassis-log.c
	chassis-mainloop.c
	chassis-keyfile.c
	chassis-path.c
	chassis-stats.c
)

SET(proxy_sources 
	network-mysqld.c 
	network-mysqld-lua.c 
	network-mysqld-proto.c 
	network-mysqld-binlog.c 
	network-mysqld-packet.c 
	network-mysqld-masterinfo.c 
	network-conn-pool.c  
	network-conn-pool-lua.c  
	network-socket.c
	network-socket-lua.c
	network-address.c
	network-address-lua.c
	network-injection.c
	network-injection-lua.c
	network-backend.c
	network-backend-lua.c
	lua-env.c
)

ADD_LIBRARY(mysql-chassis SHARED ${chassis_sources})
ADD_LIBRARY(mysql-chassis-proxy SHARED ${proxy_sources})
ADD_EXECUTABLE(mysql-proxy chassis.c)

TARGET_LINK_LIBRARIES(mysql-chassis 
	${GLIB_LIBRARIES} 
	${GMODULE_LIBRARIES} 
	${GTHREAD_LIBRARIES} 
	${LUA_LIBRARIES}
	${EVENT_LIBRARIES}
)

TARGET_LINK_LIBRARIES(mysql-chassis-proxy
	mysql-chassis
)

TARGET_LINK_LIBRARIES(mysql-proxy 
	${GLIB_LIBRARIES} 
	${GMODULE_LIBRARIES} 
	${GTHREAD_LIBRARIES} 
	${LUA_LIBRARIES}
	${EVENT_LIBRARIES}
	mysql-chassis
	mysql-chassis-proxy
)

IF(WIN32)
	ADD_EXECUTABLE(mysql-proxy-svc chassis.c)
	TARGET_LINK_LIBRARIES(mysql-proxy-svc
		${GLIB_LIBRARIES} 
		${GMODULE_LIBRARIES} 
		${GTHREAD_LIBRARIES} 
		${LUA_LIBRARIES}
		${EVENT_LIBRARIES}
		mysql-chassis
		mysql-chassis-proxy
	)
	SET_TARGET_PROPERTIES(mysql-proxy-svc
		PROPERTIES
		COMPILE_DEFINITIONS WIN32_AS_SERVICE
	)
ENDIF(WIN32)

IF(WIN32)
	# Windows finds the dlls in bin/
	INSTALL(TARGETS mysql-proxy 
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib/mysql-proxy
	)
ELSE(WIN32)
	# Unix platforms provide a wrapper script to avoid relinking at install time
	
	# figure out the correct name of the shared linker lookup path for this system, default to LD_LIBRARY_PATH
	SET(DYNLIB_PATH_VAR "LD_LIBRARY_PATH")
	
	IF("AIX" STREQUAL ${CMAKE_SYSTEM_NAME})
		SET(DYNLIB_PATH_VAR "LIBPATH")
	ENDIF("AIX" STREQUAL ${CMAKE_SYSTEM_NAME})
	
	IF("HP-UX" STREQUAL ${CMAKE_SYSTEM_NAME})
		SET(DYNLIB_PATH_VAR "SHLIB_PATH")
	ENDIF("HP-UX" STREQUAL ${CMAKE_SYSTEM_NAME})
	
	IF(APPLE)
		SET(DYNLIB_PATH_VAR "DYLD_LIBRARY_PATH")
	ENDIF(APPLE)
	
	# write the wrapper script, which uses DYNLIB_PATH_VAR
	CONFIGURE_FILE(
		${CMAKE_SOURCE_DIR}/scripts/mysql-proxy-binwrapper.in 
		${PROJECT_BINARY_DIR}/mysql-proxy.sh
		@ONLY
	)
	INSTALL(FILES ${PROJECT_BINARY_DIR}/mysql-proxy.sh
		DESTINATION bin/
		PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
		RENAME mysql-proxy
	)

	INSTALL(TARGETS mysql-proxy
		RUNTIME DESTINATION libexec
	)
ENDIF(WIN32)

INSTALL(TARGETS mysql-chassis 
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)

INSTALL(TARGETS mysql-chassis-proxy 
	RUNTIME DESTINATION bin
	LIBRARY DESTINATION lib
)

