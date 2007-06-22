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
		print("we got a normal query: " .. string.sub(packet, 2))

		proxy.queries:append(1, packet )
		proxy.queries:append(2, string.char(proxy.COM_QUERY) .. "SELECT NOW()" )

		return proxy.PROXY_SEND_QUERY
	end
end

---
-- read_query_result() is called when we receive a query result 
-- from the server
--
-- we can analyze the response, drop the response and pass it on (default)
--
-- as we injected a SELECT NOW() above, we don't want to tell the client about it
-- and drop the result with proxy.PROXY_IGNORE_RESULT
-- 
-- @return 
--   * nothing or proxy.PROXY_SEND_RESULT to pass the result-set to the client
--   * proxy.PROXY_IGNORE_RESULT to drop the result-set
-- 
function read_query_result(inj)
	print("injected result-set: id = " .. inj.type)

	if (inj.type == 2) then
		for row in inj.resultset.rows do
			print("injected query returned: " .. row[0])
		end

		return proxy.PROXY_IGNORE_RESULT
	end
end
