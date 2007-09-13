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
		auto_filter = true,
		auto_explain = false,
		auto_processlist = false,
		min_queries = 100
	}
end

function math.rolling_avg_init()
	return { count = 0, value = 0 }
end

function math.rolling_stddev_init()
	return { count = 0, value = 0, sum_x = 0, sum_x_sqr = 0 }
end

function math.rolling_avg(val, tbl) 
	tbl.count = tbl.count + 1

	-- scale the old avg to the full-sum
	-- add the current val and divide it against the new count
	tbl.value = ((tbl.value * (tbl.count - 1)) + val) / tbl.count

	return tbl.value
end


function math.rolling_stddev(val, tbl)
	tbl.sum_x     = tbl.sum_x + val
	tbl.sum_x_sqr = tbl.sum_x_sqr + (val * val)
	tbl.count     = tbl.count + 1

	tbl.value = math.sqrt((tbl.count * tbl.sum_x_sqr - (tbl.sum_x * tbl.sum_x)) / (tbl.count * (tbl.count - 1)))

	return tbl.value
end

---
-- init query counters
-- 
-- the normalized queries are 
if not proxy.global.queries then
	proxy.global.queries = { }
end

if not proxy.global.baseline then
	proxy.global.baseline = {
		avg = math.rolling_avg_init(),
		stddev = math.rolling_stddev_init()
	}
end


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

	-- we are connection-global
	baseline = nil
	log_query = false
	tokens = tokenizer.tokenize(cmd.query)
	norm_query = tokenizer.normalize(tokens)

	---
	-- do we want to analyse this query ?
	--
	--
	local queries = proxy.global.queries

	if not queries[norm_query] then
		queries[norm_query] = {
			query = norm_query,
			avg = math.rolling_avg_init(),
			stddev = math.rolling_stddev_init()
		}
	end


	if queries[norm_query].do_analyze then
		-- cover the query in SHOW SESSION STATUS
		proxy.queries:append(2, string.char(proxy.COM_QUERY) .. "SHOW SESSION STATUS")
		proxy.queries:append(1, packet)
		proxy.queries:append(3, string.char(proxy.COM_QUERY) .. "SHOW SESSION STATUS")
	else
		proxy.queries:append(1, packet)
	end

	return proxy.PROXY_SEND_QUERY
end

---
-- normalize a timestamp into a string
--
-- @param ts time in microseconds
-- @return a string with a useful suffix
function normalize_time(ts)
	local suffix = "us"
	if ts > 10000 then
		ts = ts / 10000
		suffix = "ms"
	end
	
	if ts > 10000 then
		ts = ts / 10000
		suffix = "s"
	end

	return string.format("%.2f", ts) .. " " .. suffix
end

function read_query_result(inj)
	local res = assert(inj.resultset)
	local auto_filter = proxy.global.config.analyze_query.auto_filter

	if inj.id == 1 then
		log_query = false
		if not res.query_status or res.query_status == proxy.MYSQLD_PACKET_ERR then
			-- the query failed, let's clean the queue
			--
			proxy.queries:reset()
		end

		-- get the statistics values for the query
		local bl = proxy.global.baseline
		local avg_query_time    = math.rolling_avg(   inj.query_time, bl.avg)
		local stddev_query_time = math.rolling_stddev(inj.query_time, bl.stddev)

		local st = proxy.global.queries[norm_query]
		local q_avg_query_time    = math.rolling_avg(   inj.query_time, st.avg)
		local q_stddev_query_time = math.rolling_stddev(inj.query_time, st.stddev)

		o = "# " .. os.date("%Y-%m-%d %H:%M:%S") .. 
			" [".. proxy.connection.server.thread_id .. 
			"] user: " .. proxy.connection.client.username .. 
			", db: " .. proxy.connection.client.default_db .. "\n" ..
			"  Query:                     " .. string.format("%q", inj.query:sub(2)) .. "\n" ..
			"  Norm_Query:                " .. string.format("%q", norm_query) .. "\n" ..
			"  query_time:                " .. normalize_time(inj.query_time) .. "\n" ..
			"  global(avg_query_time):    " .. normalize_time(avg_query_time) .. "\n" ..
			"  global(stddev_query_time): " .. normalize_time(stddev_query_time) .. "\n" ..
			"  global(count):             " .. bl.avg.count .. "\n" ..
			"  query(avg_query_time):     " .. normalize_time(q_avg_query_time) .. "\n" ..
			"  query(stddev_query_time):  " .. normalize_time(q_stddev_query_time) .. "\n" ..
			"  query(count):              " .. st.avg.count .. "\n" 


		-- this query is slower than 95% of the average
		if bl.avg.count > proxy.global.config.analyze_query.min_queries and
		   inj.query_time > avg_query_time + 2 * stddev_query_time then
			log_query = true
		end
	
		-- this query was slower than 95% of its kind
		if st.avg.count > proxy.global.config.analyze_query.min_queries and 
		   inj.query_time > q_avg_query_time + 2 * q_stddev_query_time then
			log_query = true
		end

		if log_query and proxy.global.config.analyze_query.auto_processlist then
			proxy.queries:append(4, string.char(proxy.COM_QUERY) .. "SHOW FULL PROCESSLIST")
		end
		
		if log_query and proxy.global.config.analyze_query.auto_explain then
			if tokens[0].token_name == "TK_SQL_SELECT" then
				proxy.queries:append(4, string.char(proxy.COM_QUERY) .. "EXPLAIN " .. inj.query)
			end
		end

		-- there is nothing in the queue, we are the last query 
		-- print everything to the log
		if log_query and proxy.queries:len() == 0 then
			print(o)
			o = nil
			log_query = false
		end

		return
	elseif inj.id == 2 then
		-- the first SHOW SESSION STATUS
		baseline = {}
		
		for row in res.rows do
			-- 1 is the key, 2 is the value
			baseline[row[1]] = row[2]
		end
	elseif inj.id == 3 then
		local delta_counters = { }
		
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
					delta_counters[row[1]] = (num2 - num1)
				end
			end
		end
		baseline = nil
	
		--- filter data
		-- 
		-- we want to see queries which 
		-- - trigger a tmp-disk-tables
		-- - are slower than the average
		--

		if delta_counters["Created_tmp_disk_tables"] then
			log_query = true
		end

		if log_query then
			for k, v in pairs(delta_counters) do
				o = o .. ".. " .. row[1] .. " = " .. (num2 - num1) .. "\n"
			end
			-- add a newline
			print(o)
		end
	end

	return proxy.PROXY_IGNORE_RESULT
end
