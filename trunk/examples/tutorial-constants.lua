--[[

   Copyright (C) 2007, 2008 MySQL AB

--]]

-- we need at least 0.5.1
assert(proxy.PROXY_VERSION >= 0x00501, "you need at least mysql-proxy 0.5.1 to run this module")

---
-- read_query() gets the client query before it reaches the server
--
-- @param packet the mysql-packet sent by client
--
-- we have several constants defined, e.g.:
-- * _VERSION
--
-- * proxy.PROXY_VERSION
-- * proxy.COM_* 
-- * proxy.MYSQL_FIELD_*
--
function read_query( packet )
	if packet:byte() == proxy.COM_QUERY then
		print("get got a Query: " .. packet:sub(2))

		-- proxy.PROXY_VERSION is the proxy version as HEX number
		-- 0x00501 is 0.5.1 
		print("we are: " .. string.format("%05x", proxy.PROXY_VERSION))
		print("lua is: " .. _VERSION)
	end
end

