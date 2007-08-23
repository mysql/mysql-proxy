function read_query(packet) 
	if packet:byte() == proxy.COM_QUERY then
		local q = packet:sub(2) 

		if q == "SELECT 1 /* BUG #29494 */" then
			-- create a packet which is will break the client
			--
			-- HINT: lua uses \ddd (3 decimal digits) instead of octals
			proxy.response.type = proxy.MYSQLD_PACKET_RAW
			proxy.response.packets = {
				"\001",  -- one field
				"\003def" ..   -- catalog
				  "\251" ..  -- db, NULL (crashes client)
				  "\0" ..    -- table
				  "\0" ..    -- orig-table
				  "\0011" .. -- name
				  "\0" ..    -- orig-name
				  "\f" ..    -- filler
				  "\008\0" .. -- charset
				  " \0\0\0" .. -- length
				  "\003" ..    -- type
				  "\002\0" ..  -- flags 
				  "\0" ..    -- decimals
				  "\0\0",    -- filler

				"\254\0\0\002\0", -- EOF
				"\254\0\0\002\0"  -- no data EOF
			}
			
			return proxy.PROXY_SEND_RESULT
		elseif q == "SELECT 1" then
			-- return a empty row
			--
			-- HINT: lua uses \ddd (3 decimal digits) instead of octals
			proxy.response.type = proxy.MYSQLD_PACKET_RAW
			proxy.response.packets = {
				"\001",  -- one field
				"\003def" ..   -- catalog
				  "\0" ..    -- db 
				  "\0" ..    -- table
				  "\0" ..    -- orig-table
				  "\0011" .. -- name
				  "\0" ..    -- orig-name
				  "\f" ..    -- filler
				  "\008\0" .. -- charset
				  " \0\0\0" .. -- length
				  "\003" ..    -- type
				  "\002\0" ..  -- flags 
				  "\0" ..    -- decimals
				  "\0\0",    -- filler

				"\254\0\0\002\0", -- EOF
				"\0011",
				"\254\0\0\002\0"  -- no data EOF
			}
			
			return proxy.PROXY_SEND_RESULT
		end
	end
end
