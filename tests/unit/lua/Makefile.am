TESTS=proxy-test.lua \
      active-queries.lua \
      proxy-auto-config.lua \
      proxy-commands.lua \
      proxy-tokenizer.lua \
      proxy-parser.lua \
      mysql-proto.lua \
      chassis.lua

TESTS_ENVIRONMENT = \
	LUA_PATH="${top_srcdir}/lib/?.lua" \
	LUA_CPATH="${top_builddir}/lib/.libs/?.so" \
	DYLD_LIBRARY_PATH="${top_builddir}/src/.libs/" \
	lua 

EXTRA_DIST=$(TESTS)
