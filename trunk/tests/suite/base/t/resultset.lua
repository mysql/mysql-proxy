function read_query(packet)
	if packet:sub(2) == "test_res_blob" then
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
					{ string.rep("x", 300) }
				}
			}
		}
	else
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
			errmsg = "unknown query"
		}
	end

	return proxy.PROXY_SEND_RESULT
end
