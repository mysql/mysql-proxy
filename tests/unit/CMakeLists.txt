ADD_SUBDIRECTORY(lua)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/src/)
INCLUDE_DIRECTORIES(${PROJECT_BINARY_DIR}) # for config.h

INCLUDE_DIRECTORIES(${GLIB_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${MYSQL_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${LUA_INCLUDE_DIRS})
INCLUDE_DIRECTORIES(${EVENT_INCLUDE_DIRS})

LINK_DIRECTORIES(${MYSQL_LIBRARY_DIRS})
LINK_DIRECTORIES(${LUA_LIBRARY_DIRS})
LINK_DIRECTORIES(${GLIB_LIBRARY_DIRS})
LINK_DIRECTORIES(${EVENT_LIBRARY_DIRS})

SET(WINSOCK_LIBRARIES)
IF(WIN32)
	SET(WINSOCK_LIBRARIES ws2_32)
ENDIF()
## tests for the chassis

ADD_EXECUTABLE(check_chassis_log
	check_chassis_log.c 
)
TARGET_LINK_LIBRARIES(check_chassis_log 
	mysql-chassis
	${GLIB_LIBRARIES}
	${GTHREAD_LIBRARIES}
	${GMODULE_LIBRARIES} 
)
ADD_EXECUTABLE(check_plugin
	check_plugin.c 
	../../src/chassis-plugin.c
)
TARGET_LINK_LIBRARIES(check_plugin 
	${GLIB_LIBRARIES}
	${GTHREAD_LIBRARIES}
	${GMODULE_LIBRARIES}
)
ADD_EXECUTABLE(check_mysqld_proto 
	check_mysqld_proto.c 
	../../src/glib-ext.c
	../../src/network-mysqld-proto.c
	../../src/network-mysqld-binlog.c
)
TARGET_LINK_LIBRARIES(check_mysqld_proto
	${GLIB_LIBRARIES}
)

## this test needs a existing sql-tokenizer.c ... 
## it depends on the build-order if that is already generated
## or not
#ADD_EXECUTABLE(check_sql_tokenizer
#	check_sql_tokenizer.c 
#	../../build-src/sql-tokenizer.c
#	)

#TARGET_LINK_LIBRARIES(check_sql_tokenizer
#	${GLIB_LIBRARIES}
#)

ADD_EXECUTABLE(check_loadscript
	check_loadscript.c 
	../../src/lua-scope.c 
	../../src/lua-load-factory.c
	../../src/chassis-stats.c 
)

TARGET_LINK_LIBRARIES(check_loadscript
	${GLIB_LIBRARIES}
	${GTHREAD_LIBRARIES}
	${LUA_LIBRARIES}
)


ADD_EXECUTABLE(check_chassis_path
	check_chassis_path.c 
	../../src/chassis-log.c 
	../../src/chassis-event-thread.c
	../../src/chassis-mainloop.c 
	../../src/chassis-plugin.c
	../../src/chassis-stats.c 
	../../src/chassis-path.c
)

TARGET_LINK_LIBRARIES(check_chassis_path
	${GLIB_LIBRARIES}
	${GTHREAD_LIBRARIES}
	${GMODULE_LIBRARIES} 
	${EVENT_LIBRARIES}
	${WINSOCK_LIBRARIES}
)

ADD_EXECUTABLE(t_network_injection
	t_network_injection.c 
	../../src/network-injection.c 
	../../src/glib-ext.c 
	../../src/network-mysqld-proto.c 
	../../src/network-mysqld-packet.c 
)

TARGET_LINK_LIBRARIES(t_network_injection
	${GLIB_LIBRARIES}
	${GTHREAD_LIBRARIES}
	${EVENT_LIBRARIES}
)

ADD_EXECUTABLE(t_network_backend
	t_network_backend.c
	../../src/network-backend.c
	../../src/network-conn-pool.c
	../../src/network-socket.c
	../../src/glib-ext.c
	../../src/network-mysqld-proto.c
	../../src/network-mysqld-packet.c
	../../src/network-address.c
)

TARGET_LINK_LIBRARIES(t_network_backend
	${GLIB_LIBRARIES}
	${GTHREAD_LIBRARIES}
	${EVENT_LIBRARIES}
	${WINSOCK_LIBRARIES}
)

IF(WIN32)
# turn off _declspec(dllimport) in tests, since we link statically
set_property(TARGET check_chassis_log check_plugin check_mysqld_proto check_loadscript check_chassis_path t_network_injection t_network_backend
             APPEND PROPERTY COMPILE_DEFINITIONS "mysql_chassis_proxy_STATIC" COMPILE_DEFINITIONS "mysql_chassis_STATIC")
ENDIF(WIN32)

## adding all tests

ADD_TEST(check_chassis_log check_chassis_log)
ADD_TEST(check_plugin check_plugin)
ADD_TEST(check_mysqld_proto check_mysqld_proto)
#ADD_TEST(check_sql_tokenizer check_sql_tokenizer)
ADD_TEST(check_loadscript check_loadscript)
ADD_TEST(check_chassis_path check_chassis_path)
ADD_TEST(t_network_injection t_network_injection)
ADD_TEST(t_network_backend t_network_backend)

