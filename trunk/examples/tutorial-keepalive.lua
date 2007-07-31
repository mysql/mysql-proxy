
local is_in_transaction = 0
local is_debug = false

function connect_server() 
	-- make sure that we connect to each backend at least ones to 
	-- keep the connections to the servers alive
	--
	-- on read_query we can switch the backends again to another backend

	if is_debug then
		print()
		print("[connect_server] ")
	end

	for i = 1, #proxy.servers do
		local s = proxy.servers[i]
		if is_debug then
			print("  [".. i .."].connected_clients = " .. s.connected_clients)
			print("  [".. i .."].idling_connections = " .. s.idling_connections)
			print("  [".. i .."].type = " .. s.type)
			print("  [".. i .."].state = " .. s.state)
		end

		if s.idling_connections == 0 and s.state ~= 2 then
			proxy.connection.backend_ndx = i
			break
		end
	end
end

function read_auth_result(packet) 
	-- disconnect from the server
	proxy.connection.backend_ndx = 0
end

function read_query( packet ) 
	if is_debug then
		print("[read_query]")
		print("  authed backend = " .. proxy.connection.backend_ndx)
		print("  used db = " .. proxy.connection.default_db)
	end

	if packet:byte() == proxy.COM_QUIT then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
			errmsg = "ignored the COM_QUIT"
		}

		return proxy.PROXY_SEND_RESULT
	end

	if proxy.connection.backend_ndx == 0 then
		-- pick a master
		for i = 1, #proxy.servers do
			local s = proxy.servers[i]
			if s.idling_connections > 0 and s.state ~= 2 and s.type == 1 then
				proxy.connection.backend_ndx = i
				break
			end
		end
	end

	proxy.queries:append(1, packet)

	if is_in_transaction == 0 and
	   packet:byte() == proxy.COM_QUERY and
	   packet:sub(2, 7) == "SELECT" then
		local max_conns = -1
		local max_conns_ndx = 0

		for i = 1, #proxy.servers do
			local s = proxy.servers[i]

			if s.type == 2 and s.idling_connections > 0 then
				if max_conns == -1 or 
				   s.connected_clients < max_conns then
					max_conns = s.connected_clients
					max_conns_ndx = i
				end
			end
		end
		-- send to slave
		if max_conns_ndx > 0 then
			proxy.connection.backend_ndx = max_conns_ndx
			proxy.queries:prepend(2, "\002" .. proxy.connection.default_db)
			if is_debug then
				print("  sending ".. string.format("%q", packet:sub(2)) .. " to slave: " .. proxy.connection.backend_ndx);
			end
		end
	else
		-- send to master
		if is_debug then
			print("  sending to master: " .. proxy.connection.backend_ndx);
		end
	end

	return proxy.PROXY_SEND_QUERY
end

function read_query_result( inj ) 
	local res      = assert(inj.resultset)
  	local flags    = res.flags

	if inj.id ~= 1 then
		return proxy.PROXY_IGNORE_RESULT
	end
	is_in_transaction = flags.in_trans

	if is_in_transaction == 0 then
		-- release the backend
		proxy.connection.backend_ndx = 0
	end
end

