---
-- test the proxy.auto-config module
--
-- 

local tests      = require("proxy.test")
local command    = require("proxy.commands")
tests.ProxyBaseTest.setDefaultScope()

-- file under test
local tokenizer = require("proxy.tokenizer")

TestScript = tests.ProxyBaseTest:new()

function TestScript:setUp()
	proxy.global.config.test = { }
end

function TestScript:testNormalize()
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

	local tokens = tokenizer.tokenize(cmd);
	local norm_query = tokenizer.normalize(tokens)

	assertEquals(norm_query, "SET `GLOBAL` `unknown` . `unknown` = ? ")
end


---
-- the test suite runner

local suite = tests.Suite:new({ result = tests.Result:new()})

suite:run()
suite.result:print()
return suite:exit_code()
