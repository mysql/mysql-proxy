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
	proxy.response = {
		type = proxy.MYSQLD_PACKET_OK
	}
	return proxy.PROXY_SEND_RESULT
end




