--[[ $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ --]]


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
