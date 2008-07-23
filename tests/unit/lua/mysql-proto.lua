local proto = assert(require("mysql.proto"))

---
-- err packet

local err_packet = proto.to_err_packet({ errmsg = "123" })
assert(type(err_packet) == "string")
assert(#err_packet == 12)

local tbl = proto.from_err_packet(err_packet)
assert(tbl.errmsg == "123")
assert(tbl.errcode == 0)
assert(tbl.sqlstate == "07000")

---
-- ok packet

local ok_packet = proto.to_ok_packet({ server_status = 2 })
assert(type(ok_packet) == "string")
assert(#ok_packet == 7)
local tbl = proto.from_ok_packet(ok_packet)
assert(tbl.server_status == 2)
assert(tbl.insert_id == 0)
assert(tbl.affected_rows == 0)
assert(tbl.warnings == 0)

-- should fail
assert(false == pcall(
	-- wrong type
	function () 
		proto.from_ok_packet(nil)
	end
))

assert(false == pcall(
	-- packet is too short
	function () 
		proto.from_ok_packet("")
	end
))

-- should decode nicely
assert(true == pcall(
	function () 
		proto.from_ok_packet("\000\000\000\002\000\000\000")
	end
))

---
-- eof packet

local eof_packet = proto.to_eof_packet({ server_status = 2 })
assert(type(eof_packet) == "string")
assert(#eof_packet == 5)
local tbl = proto.from_eof_packet(eof_packet)
assert(tbl.server_status == 2)
assert(tbl.warnings == 0)

-- should fail
assert(false == pcall(
	-- wrong type
	function () 
		proto.from_eof_packet(nil)
	end
))

assert(false == pcall(
	-- packet is too short
	function () 
		proto.from_eof_packet("")
	end
))

-- should decode nicely
assert(true == pcall(
	function () 
		proto.from_eof_packet("\254\002\000\000\000")
	end
))

---
-- challenge packet

-- should fail
assert(false == pcall(
	-- wrong type
	function () 
		proto.from_challenge_packet(nil)
	end
))

assert(false == pcall(
	-- packet is too short
	function () 
		proto.from_challenge_packet("")
	end
))

-- should decode nicely
assert(true == pcall(
	function () 
		proto.from_challenge_packet(
			"\010"..
			"5.0.24\000"..
			"\000\000\000\000".. 
			"01234567" ..
			"\000" ..
			"\000\000" ..
			"\000" ..
			"\000\000" ..
			("\000"):rep(13) ..
			("."):rep(12) ..
			"\000"
			)
	end
))

local challenge_packet = proto.to_challenge_packet({ server_status = 2, server_version = 50034 })
assert(type(challenge_packet) == "string")
assert(#challenge_packet == 53, ("expected 53, got %d"):format(#challenge_packet))
local tbl = proto.from_challenge_packet(challenge_packet)
assert(type(tbl) == "table")
assert(tbl.server_status == 2, ("expected 2, got %d"):format(tbl.server_status))
assert(tbl.server_version == 50034, ("expected 50034, got %d"):format(tbl.server_version))

---
-- response packet

-- should fail
assert(false == pcall(
	-- wrong type
	function () 
		proto.from_response_packet(nil)
	end
))

assert(false == pcall(
	-- packet is too short
	function () 
		proto.from_response_packet("")
	end
))

-- should decode nicely
assert(true == pcall(
	function () 
		proto.from_response_packet(
			"\000\000\000\000".. 
			"\000\000\000\000".. 
			"\000" ..
			("\000"):rep(23) ..
			"01234567" ..  "\000" ..
			"\020"..("."):rep(20) ..
			"\000" ..
			"foobar" .. "\000"

			)
	end
))

local response_packet = proto.to_response_packet({ username = "foobar", database = "db" })
assert(type(response_packet) == "string")
assert(#response_packet == 43, ("expected 43, got %d"):format(#response_packet))
local tbl = proto.from_response_packet(response_packet)
assert(type(tbl) == "table")
assert(tbl.username == "foobar", ("expected 'foobar', got %s"):format(tostring(tbl.username)))
assert(tbl.database == "db", ("expected 'db', got %s"):format(tostring(tbl.database)))


