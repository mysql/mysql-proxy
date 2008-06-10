---
-- test the proxy.auto-config module
--
-- 

local tests      = require("proxy.test")
local command    = require("proxy.commands")
tests.ProxyBaseTest.setDefaultScope()

-- file under test
local tokenizer = require("proxy.tokenizer")
local parser = require("proxy.parser")

TestScript = tests.ProxyBaseTest:new()

function TestScript:setUp()
	proxy.global.config.test = { }
end

function TestScript:testGetTables()
	local cmd = command.parse(string.char(proxy.COM_QUERY) .. "SELECT * FROM db.tbl")

	-- mock the proxy.tokenize function
	proxy.tokenize = function (str) 
		return { 
			{ token_name = "TK_SQL_SELECT", text = "SELECT" },
			{ token_name = "TK_STAR", text = "*" },
			{ token_name = "TK_SQL_FROM", text = "FROM" },
			{ token_name = "TK_LITERAL", text = "db" },
			{ token_name = "TK_DOT",     text = "." },
			{ token_name = "TK_LITERAL", text = "tbl" },
		}
	end

	local tokens = tokenizer.tokenize(cmd);
	local tables = parser.get_tables(tokens)

	assertEquals(tables["db.tbl"], "read") -- and it is db.tbl
end


---
-- the test suite runner

local suite = tests.Suite:new({ result = tests.Result:new()})

suite:run()
suite.result:print()
return suite:exit_code()
