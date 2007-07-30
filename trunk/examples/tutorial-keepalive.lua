
local is_in_transaction = 0

function connect_server() 
	-- make sure that we connect to each backend at least ones to 
	-- keep the connections to the servers alive
	--
	-- on read_query we can switch the backends again to another backend

	print()
	print("[connect_server] ")

	for i = 1, #proxy.servers do
		local s = proxy.servers[i]
		print("  [".. i .."].connected_clients = " .. s.connected_clients)
		print("  [".. i .."].idling_connections = " .. s.idling_connections)
		print("  [".. i .."].type = " .. s.type)

		if s.idling_connections == 0 then
			proxy.connection.backend_ndx = i
			break
		end
	end
end

function read_query( packet ) 
	print("[read_query]")
	print("  authed backend = " .. proxy.connection.backend_ndx)

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
		for i = 1, #proxy.servers do
			local s = proxy.servers[i]

			if s.type == 2 and s.idling_connections > 0 then
				proxy.connection.backend_ndx = i
				break
			end
		end
		-- send to slave
		print("  sending ".. string.format("%q", packet:sub(2)) .. " to slave: " .. proxy.connection.backend_ndx);
	else
		-- send to master
		print("  sending to master: " .. proxy.connection.backend_ndx);
	end

	return proxy.PROXY_SEND_QUERY
end

function read_query_result( inj ) 
	local res      = assert(inj.resultset)
  	local flags    = res.flags

	is_in_transaction = flags.in_trans
end

