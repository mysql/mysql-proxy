---
-- test the proxy.auto-config module
--
-- 

local tests      = require("proxy.test")
local command    = require("proxy.commands")
tests.ProxyBaseTest.setDefaultScope()

-- file under test
local autoconfig = require("proxy.auto-config")

TestScript = tests.ProxyBaseTest:new()

function TestScript:setUp()
	self:setDefaultScope()

	proxy.global.config.test = { }
end

function TestScript:testUnknownOption()
	local cmd = command.parse(string.char(proxy.COM_QUERY) .. "PROXY SET GLOBAL unknown.unknown = 1")

	local r = autoconfig.handle(proxy.global.config, cmd)

	assertEquals(r, proxy.PROXY_SEND_RESULT)
	assertEquals(proxy.response.type, proxy.MYSQLD_PACKET_ERR)
	assert(proxy.response.errmsg > "")
end

function TestScript:testKnownModule()
	local cmd = command.parse(string.char(proxy.COM_QUERY) .. "PROXY SET GLOBAL test.unknown = 1")

	local r = autoconfig.handle(proxy.global.config, cmd)

	assertEquals(proxy.response.type, proxy.MYSQLD_PACKET_ERR)
	assert(proxy.response.errmsg > "")
end

function TestScript:testShowConfig()
	local cmd = command.parse(string.char(proxy.COM_QUERY) .. "PROXY SHOW CONFIG")

	local r = autoconfig.handle(proxy.global.config, cmd)

	assertEquals(r, proxy.PROXY_SEND_RESULT)
end


function TestScript:testKnownModule()
	local cmd = command.parse(string.char(proxy.COM_QUERY) .. "PROXY SET GLOBAL test.option = 1")

	proxy.global.config.test.option = 0

	local r = autoconfig.handle(proxy.global.config, cmd)

	assertEquals(r, proxy.PROXY_SEND_RESULT)
	assertEquals(proxy.response.type, proxy.MYSQLD_PACKET_OK)
	assertEquals(proxy.global.config.test.option, 1)
end

function TestScript:testKnownModuleWrongType()
	local cmd = command.parse(string.char(proxy.COM_QUERY) .. "PROXY SET GLOBAL test.option = \"foo\"")

	proxy.global.config.test.option = 0

	local r = autoconfig.handle(proxy.global.config, cmd)

	assertEquals(r, proxy.PROXY_SEND_RESULT)
	assertEquals(proxy.response.type, proxy.MYSQLD_PACKET_ERR)
	assert(proxy.response.errmsg > "")
end


---
-- the test suite runner

local suite = tests.Suite:new({ result = tests.Result:new()})

suite:run()
suite.result:print()
os.exit(suite:exit_code())
