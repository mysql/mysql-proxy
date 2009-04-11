EXTRA_DIST=

example_scripts = 
	tutorial-basic.lua \
	tutorial-inject.lua \
	tutorial-rewrite.lua \
	tutorial-query-time.lua \
	tutorial-packets.lua \
	tutorial-warnings.lua \
	tutorial-union.lua \
	tutorial-constants.lua \
	tutorial-keepalive.lua \
	tutorial-tokenize.lua \
	tutorial-resultset.lua \
	tutorial-states.lua \
	tutorial-routing.lua \
	tutorial-monitor.lua

if USE_WRAPPER_SCRIPT
dist_doc_DATA = ${example_scripts}
else
EXTRA_DIST += ${example_scripts}
endif

# EXTRA_DIST=CMakeLists.txt
