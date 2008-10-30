--[[ $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ --]]
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

	local tokens = tokenizer.tokenize(cmd.query);
	local tables = parser.get_tables(tokens)

	assertEquals(tables["db.tbl"], "read") -- and it is db.tbl
end


---
-- the test suite runner

local suite = tests.Suite:new({ result = tests.Result:new()})

suite:run()
suite.result:print()
return suite:exit_code()
