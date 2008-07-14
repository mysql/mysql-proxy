local proto = require("mysql.proto")

function connect_server()
	-- emulate a server
	proxy.response = {
		type = proxy.MYSQLD_PACKET_RAW,
		packets = {
			proto.to_challenge_packet({})
		}
	}
	return proxy.PROXY_SEND_RESULT
end

function read_query(packet)
	if packet:byte() ~= proxy.COM_QUERY then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK
		}
		return proxy.PROXY_SEND_RESULT
	end

	local query = packet:sub(2) 
	if query == 'SELECT 1' then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = '1' },
				},
				rows = { { 1 } }
			}
		}
	elseif query == 'SELECT ' then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
			errmsg = "You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '' at line 1",
			sqlstate = "42000",
			errcode = 1064
		}
	elseif query == 'test_res_blob' then
		-- we need a long string, more than 255 chars
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = { 
				fields = { 
					{
						name = "300x",
						type = proxy.MYSQL_TYPE_BLOB
					}
				},
				rows = {
					{ ("x"):rep(300) }
				}
			}
		}
	elseif query == 'affected_rows' then
		-- we need a long string, more than 255 chars
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			affected_rows = 2,
			insert_id = 10
		}
	else
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
			errmsg = "(resultset-mock) >" .. query .. "<"
		}
	end
	return proxy.PROXY_SEND_RESULT
end




