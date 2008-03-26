--[[

   Copyright (C) 2007, 2008 MySQL AB

--]]

---
-- read_query() can rewrite packets
--
-- You can use read_query() to replace the packet sent by the client and rewrite
-- query as you like
--
-- @param packet the mysql-packet sent by the client
--
-- @return 
--   * nothing to pass on the packet as is, 
--   * proxy.PROXY_SEND_QUERY to send the queries from the proxy.queries queue
--   * proxy.PROXY_SEND_RESULT to send your own result-set
--
function read_query( packet )
	if string.byte(packet) == proxy.COM_QUERY then
		proxy.queries:append(1, packet )

		return proxy.PROXY_SEND_QUERY
	end
end

---
-- dumps the warnings of queries
-- 
-- read_query_result() is called when we receive a query result 
-- from the server
--
-- for all queries which pass by we check if the warning-count is > 0 and
-- inject a SHOW WARNINGS and dump it to the stdout
--
-- @return 
--   * nothing or proxy.PROXY_SEND_RESULT to pass the result-set to the client
--   * proxy.PROXY_IGNORE_RESULT to drop the result-set
-- 
function read_query_result(inj)

	if (inj.id == 1) then
  		local res = assert(inj.resultset)

		if res.warning_count > 0 then
			print("Query had warnings: " .. inj.query:sub(2))
			proxy.queries:append(2, string.char(proxy.COM_QUERY) .. "SHOW WARNINGS" )
		end
	elseif (inj.id == 2) then
		for row in inj.resultset.rows do
			print(string.format("warning: [%d] %s", row[1], row[2]))
		end

		return proxy.PROXY_IGNORE_RESULT
	end
end

