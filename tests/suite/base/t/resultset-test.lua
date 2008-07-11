local chassis = require("chassis")

function read_query(packet)
	proxy.queries:append(1, packet)

	print("123")

	return proxy.PROXY_SEND_QUERY
end

function read_query_result(inj)
	assert(inj)

	local res = assert(inj.res, "no res")




	print(("handling COM_%02d"):format(inj.query:byte()))

	local status = assert(res.query_status, ("COM_%02d"):format(inj.query:byte()))

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

