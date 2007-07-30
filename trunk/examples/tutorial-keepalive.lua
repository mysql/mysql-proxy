
local is_in_transaction = 0

function connect_server() 
	-- make sure that we connect to each backend at least ones to 
	-- keep the connections to the servers alive
	--
	-- on read_query we can switch the backends again to another backend
end

function read_query( packet ) 
	if packet:byte() == proxy.COM_QUIT then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
			errmsg = "ignored the COM_QUIT"
		}

		return proxy.PROXY_SEND_RESULT
	end

	proxy.queries:append(1, packet)

	if is_in_transaction == 0 and
	   packet:byte() == proxy.COM_QUERY and
	   packet:sub(2, 7) == "SELECT" then
		-- send to slave
		print("sending to slave");
	else
		-- send to master
		print("sending to master");
	end

	return proxy.PROXY_SEND_QUERY
end

function read_query_result( inj ) 
	local res      = assert(inj.resultset)
  	local flags    = res.flags

	is_in_transaction = flags.in_trans
end

