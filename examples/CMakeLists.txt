## automake uses 
##
##   docdir = ${datarootdir}/doc/${PACKAGE_TARNAME}
##
## so do we 
INSTALL(FILES
	tutorial-basic.lua
	tutorial-constants.lua
	tutorial-inject.lua
	tutorial-keepalive.lua
	tutorial-monitor.lua
	tutorial-packets.lua
	tutorial-query-time.lua
	tutorial-prep-stmts.lua
	tutorial-resultset.lua
	tutorial-rewrite.lua
	tutorial-routing.lua
	tutorial-scramble.lua
	tutorial-states.lua
	tutorial-tokenize.lua
	tutorial-union.lua
	tutorial-warnings.lua

	DESTINATION share/doc/mysql-proxy/
)
