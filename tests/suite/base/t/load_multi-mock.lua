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

	if query == "select 'first' as info" then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = "info" },
				},
				rows = {
					{ "first" }
				}
			}
		}
	
		return proxy.PROXY_SEND_RESULT
	elseif query == "select 'second' as info" then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = "info" },
				},
				rows = {
					{ "second" }
				}
			}
		}
	
		return proxy.PROXY_SEND_RESULT
	elseif query == "select 'third' as info" then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = "info" },
				},
				rows = {
					{ "third" }
				}
			}
		}
	
		return proxy.PROXY_SEND_RESULT
	elseif query == "select 1000" then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = "1000" },
				},
				rows = {
					{ "1000" }
				}
			}
		}
	
		return proxy.PROXY_SEND_RESULT
	elseif query == "select sleep(0.2)" then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = "sleep(0.2)" },
				},
				rows = {
					{ "0" }
				}
			}
		}
	
		return proxy.PROXY_SEND_RESULT

	end

	proxy.response = {
		type = proxy.MYSQLD_PACKET_ERR,
		errmsg = ''..query..''
	}
	
	return proxy.PROXY_SEND_RESULT
end




