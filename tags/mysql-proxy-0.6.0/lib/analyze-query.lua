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

local commands     = require("proxy.commands")
local tokenizer    = require("proxy.tokenizer")
local auto_config  = require("proxy.auto-config")

if not proxy.global.config.analyze_query then
	proxy.global.config.analyze_query = {
		analyze_queries = true,
	}
end

baseline = {}

function read_query(packet) 
	local cmd = commands.parse(packet)

	local r = auto_config.handle(cmd)
	if r then return r end

	-- analyzing queries is disabled, just pass them on
	if not proxy.global.config.analyze_query then
		return
	end

	-- we only handle normal queries
	if cmd.type ~= proxy.COM_QUERY then
		return
	end

	baseline = {}

	-- cover the query in SHOW SESSION STATUS
	proxy.queries:append(2, string.char(proxy.COM_QUERY) .. "SHOW SESSION STATUS")
	proxy.queries:append(1, packet)
	proxy.queries:append(3, string.char(proxy.COM_QUERY) .. "SHOW SESSION STATUS")

	return proxy.PROXY_SEND_QUERY
end

function read_query_result(inj)
	local res = assert(inj.resultset)

	if inj.id == 1 then
		if not res.query_status or res.query_status == proxy.MYSQLD_PACKET_ERR then
			-- the query failed, let's clean the queue
			--
			proxy.queries:reset()
		end

		local tokens = tokenizer.tokenize(inj.query:sub(2))
		local norm_query = tokenizer.normalize(tokens)

		o = "# " .. os.date("%Y-%m-%d %H:%M:%S") .. 
			" [".. proxy.connection.server.thread_id .. 
			"] user: " .. proxy.connection.client.username .. 
			", db: " .. proxy.connection.client.default_db .. "\n" ..
			"Query: " .. string.format("%q", inj.query:sub(2)) .. "\n" ..
			"Norm_Query: " .. string.format("%q", norm_query) .. "\n" ..
			"Exec_time: " .. inj.query_time .. " us" .. "\n"

		return
	elseif inj.id == 2 then
		-- the first SHOW SESSION STATUS
		
		for row in res.rows do
			-- 1 is the key, 2 is the value
			baseline[row[1]] = row[2]
		end
	elseif inj.id == 3 then
		local delta_counters
		
		for row in res.rows do
			if baseline[row[1]] then
				local num1 = tonumber(baseline[row[1]])
				local num2 = tonumber(row[2])

				if row[1] == "Com_show_status" then
					num2 = num2 - 1
				elseif row[1] == "Questions" then
					num2 = num2 - 1
				elseif row[1] == "Last_query_cost" then
					num1 = 0
				end
				
				if num1 and num2 and num2 - num1 > 0 then
					o = o .. ".. " .. row[1] .. " = " .. (num2 - num1) .. "\n"
				end
			end
		end
		-- add a newline
		print(o)
	end

	return proxy.PROXY_IGNORE_RESULT
end
