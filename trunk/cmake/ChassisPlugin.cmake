MACRO(CHASSIS_PLUGIN _plugin_name)
	SET(_la_name "${CMAKE_SHARED_LIBRARY_PREFIX}${_plugin_name}.la")
	SET(_so_name "${CMAKE_SHARED_LIBRARY_PREFIX}${_plugin_name}${CMAKE_SHARED_LIBRARY_SUFFIX}")
	FILE(WRITE ${_la_name} "dlname=\"${_so_name}\"\n")
	IF(WIN32)
		FILE(APPEND ${_la_name} "libdir=\"../lib\"\n")
	ELSE(WIN32)
		FILE(APPEND ${_la_name} "libdir=\"./lib/\"\n")
	ENDIF(WIN32)

	FILE(APPEND ${_la_name} "installed=yes\n")
	
	INSTALL(FILES ${_la_name} DESTINATION lib/mysql-proxy)
ENDMACRO(CHASSIS_PLUGIN _plugin_name)

