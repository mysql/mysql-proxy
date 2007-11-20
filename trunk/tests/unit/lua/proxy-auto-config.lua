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

function mock_tokenize(query)
	-- parsing a query
	if query == "SELECT 1" then
		return {
			{ token_name = "TK_SQL_SELECT", text = "SELECT" },
			{ token_name = "TK_NUMBER",     text = "1" }
		}
	else 
		error("(mock_tokenize) for "..query)
	end
end


function TestScript:setUp()
	self:setDefaultScope()

	proxy.global.config.test = { }
	proxy.tokenize = mock_tokenize
end

function TestScript:testUnknownOption()
	local cmd = command.parse(string.char(proxy.COM_QUERY) .. "SET GLOBAL unknown.unknown = 1")

	-- mock the proxy.tokenize function
	proxy.tokenize = function (str) 
		return { 
			{ token_name = "TK_SQL_SET", text = "SET" },
			{ token_name = "TK_LITERAL", text = "GLOBAL" },
			{ token_name = "TK_LITERAL", text = "unknown" },
			{ token_name = "TK_DOT",     text = "." },
			{ token_name = "TK_LITERAL", text = "unknown" },
			{ token_name = "TK_EQ",      text = "=" },
			{ token_name = "TK_INTEGER",  text = "1" },
		}
	end

	local r = autoconfig.handle(cmd)

	assertEquals(r, nil)
end

function TestScript:testKnownModule()
	local cmd = command.parse(string.char(proxy.COM_QUERY) .. "SET GLOBAL test.unknown = 1")

	-- mock the proxy.tokenize function
	proxy.tokenize = function (str) 
		return { 
			{ token_name = "TK_SQL_SET", text = "SET" },
			{ token_name = "TK_LITERAL", text = "GLOBAL" },
			{ token_name = "TK_LITERAL", text = "test" },
			{ token_name = "TK_DOT",     text = "." },
			{ token_name = "TK_LITERAL", text = "unknown" },
			{ token_name = "TK_EQ",      text = "=" },
			{ token_name = "TK_INTEGER",  text = "1" },
		}
	end

	local r = autoconfig.handle(cmd)

	assertEquals(r, nil)
end

function TestScript:testKnownModule()
	local cmd = command.parse(string.char(proxy.COM_QUERY) .. "PROXY SET GLOBAL test.option = 1")

	-- mock the proxy.tokenize function
	proxy.tokenize = function (str) 
		return { 
			{ token_name = "TK_LITERAL", text = "PROXY" },
			{ token_name = "TK_SQL_SET", text = "SET" },
			{ token_name = "TK_LITERAL", text = "GLOBAL" },
			{ token_name = "TK_LITERAL", text = "test" },
			{ token_name = "TK_DOT",     text = "." },
			{ token_name = "TK_LITERAL", text = "option" },
			{ token_name = "TK_EQ",      text = "=" },
			{ token_name = "TK_INTEGER",  text = "1" },
		}
	end

	proxy.global.config.test.option = 0

	local r = autoconfig.handle(cmd)

	assertEquals(r, proxy.PROXY_SEND_RESULT)
	assertEquals(proxy.global.config.test.option, 1)
end

---
-- the test suite runner

local suite = tests.Suite:new({ result = tests.Result:new()})

suite:run()
suite.result:print()
return suite:exit_code()
