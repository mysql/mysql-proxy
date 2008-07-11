local chassis = require("chassis")

local my_lovely_packet

function read_query(packet)
	proxy.queries:append(1, packet)

	my_lovely_packet = packet

	return proxy.PROXY_SEND_QUERY
end

function read_query_result(inj)
	assert(inj)
	assert(inj.id == 1) -- the id we assigned above
	assert(inj.query == my_lovely_packet) -- the id we assigned above

	assert(inj.query_time > 0)
	assert(inj.response_time > 0)

	local res = assert(inj.resultset)
	local status = assert(res.query_status)

	assert(proxy.MYSQLD_PACKET_OK == 0, "OK != 0")
	assert(proxy.MYSQLD_PACKET_ERR == 255, "ERR != 255")

	if status == proxy.MYSQLD_PACKET_OK then
	elseif status == proxy.MYSQLD_PACKET_ERR then
	else
		assert(false, ("res.query_status is %d, expected %d or %d"):format(
			res.query_status,
			proxy.MYSQLD_PACKET_OK,
			proxy.MYSQLD_PACKET_ERR
		))
	end
end

