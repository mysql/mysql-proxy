--[[

   Copyright (C) 2007 MySQL AB

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

--]]

local commands = require("proxy.commands")

--- init the global scope
if not proxy.global.active_queries then
	proxy.global.active_queries = {}
end

if not proxy.global.max_active_trx then
	proxy.global.max_active_trx = 0
end


-- the connection-id is local to the script
local connection_id

---
-- track the active queries and dump all queries at each state-change
--

---
-- dump the state of the current queries
-- 
function dump_global_state()
	local o = ""
	local num_conns = 0
	local active_conns = 0

	for k, v in pairs(proxy.global.active_queries) do
		local cmd_query = ""
		if v.cmd then
			cmd_query = string.format("(%s) %q", v.cmd.type_name, v.cmd.query or "")
		end
		o = o .."  ["..k.."] (".. v.username .."@".. v.db ..") " .. cmd_query .." (state=" .. v.state .. ")\n"
		num_conns = num_conns + 1

		if v.state ~= "idle" then
			active_conns = active_conns + 1
		end
	end

	if active_conns > proxy.global.max_active_trx then
		proxy.global.max_active_trx = active_conns
	end

	-- prepend the data and the stats about the number of connections and trx
	o = os.date("%Y-%m-%d %H:%M:%S") .. "\n" ..
		"  #connections: " .. num_conns .. 
		", #active trx: " .. active_conns .. 
		", max(active trx): ".. proxy.global.max_active_trx .. 
		"\n" .. o

	print(o)
end

--- 
-- enable tracking the packets
function read_query(packet) 
	proxy.queries:append(1, packet)

	-- add the query to the global scope
	if not connection_id then
		connection_id = proxy.connection.server.thread_id
	end

	proxy.global.active_queries[connection_id] = { 
		state = "started",
		cmd = commands.parse(packet),
		db = proxy.connection.client.default_db or "",
		username = proxy.connection.client.username or ""
	}

	dump_global_state()

	return proxy.PROXY_SEND_QUERY
end

---
-- statement is done, track the change
function read_query_result(inj)
	proxy.global.active_queries[connection_id].state = "idle"
	proxy.global.active_queries[connection_id].cmd = nil
	
	if inj.resultset then
		local res = inj.resultset

		if res.flags.in_trans then
			proxy.global.active_queries[connection_id].state = "in_trans" 
		end
	end

	dump_global_state()
end

---
-- remove the information about the connection 
-- 
function disconnect_client()
	if connection_id then
		proxy.global.active_queries[connection_id] = nil
	
		dump_global_state()
	end
end

