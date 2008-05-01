
function packet_auth(fields)
	fields = fields or { }
	return "\010" ..             -- proto version
		(fields.version or "5.0.45-proxy") .. -- version
		"\000" ..             -- term-null
		"\001\000\000\000" .. -- thread-id
		"\065\065\065\065" ..
		"\065\065\065\065" .. -- challenge - part I
		"\000" ..             -- filler
		"\001\130" ..         -- server cap (long pass, 4.1 proto)
		"\008" ..             -- charset
		"\002\000" ..         -- status
		("\000"):rep(13) ..   -- filler
		"\065\065\065\065"..
		"\065\065\065\065"..
		"\065\065\065\065"..
		"\000"                -- challenge - part II
end

function connect_server()
	-- emulate a server
	proxy.response = {
		type = proxy.MYSQLD_PACKET_RAW,
		packets = {
			packet_auth()
		}
	}
	return proxy.PROXY_SEND_RESULT
end

function read_query(packet)
	if packet:byte() == proxy.COM_QUERY then
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
	elseif packet:byte() == proxy.COM_INIT_DB then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK
		}
	else
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
			errmsg = "unknown command"
		}
	end

	return proxy.PROXY_SEND_RESULT
end
