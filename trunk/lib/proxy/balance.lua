--[[

   Copyright (C) 2007, 2008 MySQL AB

--]]

module("proxy.balance", package.seeall)

function idle_failsafe_rw()
	local backend_ndx = 0

	for i = 1, #proxy.global.backends do
		local s = proxy.global.backends[i]
		local conns = s.pool.users[proxy.connection.client.username]
		
		if conns.cur_idle_connections > 0 and 
		   s.state ~= proxy.BACKEND_STATE_DOWN and 
		   s.type == proxy.BACKEND_TYPE_RW then
			backend_ndx = i
			break
		end
	end

	return backend_ndx
end

function idle_ro() 
	local max_conns = -1
	local max_conns_ndx = 0

	for i = 1, #proxy.global.backends do
		local s = proxy.global.backends[i]
		local conns = s.pool.users[proxy.connection.client.username]

		-- pick a slave which has some idling connections
		if s.type == proxy.BACKEND_TYPE_RO and 
		   s.state ~= proxy.BACKEND_STATE_DOWN and 
		   conns.cur_idle_connections > 0 then
			if max_conns == -1 or 
			   s.connected_clients < max_conns then
				max_conns = s.connected_clients
				max_conns_ndx = i
			end
		end
	end

	return max_conns_ndx
end
