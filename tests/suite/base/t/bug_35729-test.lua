---
-- Bug #35729
--
-- a value from the 1st column randomly becomes nil even if we send back
-- a value from the mock server
--
-- it only happens for the 2nd resultset we receive

---
-- duplicate the test-query
function read_query( packet )
	if packet:byte() == proxy.COM_QUERY then
		proxy.queries:append(1, packet, { resultset_is_needed = true } )
		return proxy.PROXY_SEND_QUERY
	end
end

---
-- check that the 2nd resultset is NULL-free
--
-- the underlying result is fine and contains the right data, just the Lua 
-- side gets the wrong values
--
function read_query_result(inj)
	proxy.response = {
		type = proxy.MYSQLD_PACKET_OK,
		resultset = {
			fields = {
				{ name = "Name", type = proxy.MYSQL_TYPE_STRING },
				{ name = "Value", type = proxy.MYSQL_TYPE_STRING }
			},
			rows = { }
		}
	}

	for row in inj.resultset.rows do
		collectgarbage("collect") -- trigger a full GC

		---
		-- if something goes wrong 'row' will reference a free()ed old resultset now
		-- leading to nil here
		proxy.response.resultset.rows[#proxy.response.resultset.rows + 1] = { row[1], row[2] }
	end

	return proxy.PROXY_SEND_RESULT
end
