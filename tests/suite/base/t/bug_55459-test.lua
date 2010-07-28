function read_query( packet )
	if packet:byte() == proxy.COM_QUERY then
		-- duplicate the query, but don't mark it as "send"
		proxy.queries:append(1, packet, { resultset_is_needed = true } )
	end
end

function read_query_result(inj)
	local res = assert(inj.resultset)

	if res.query_status == proxy.MYSQLD_PACKET_ERR then
		print(("received error-code: %d"):format(
			res.raw:byte(2)+(res.raw:byte(3)*256)
		))
	end
end
