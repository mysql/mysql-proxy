--[[

   Copyright (C) 2007, 2008 MySQL AB

--]]

---
-- getting the query time 
--
-- each injected query we send to the server has a start and end-time
-- 
-- * start-time: when we call proxy.queries:append()
-- * end-time:   when we received the full result-set 
--
-- @param packet the mysql-packet sent by the client
--
-- @return 
--   * proxy.PROXY_SEND_QUERY to send the queries from the proxy.queries queue
--
function read_query( packet )
	if packet:byte() == proxy.COM_QUERY then
		print("we got a normal query: " .. packet:sub(2))

		proxy.queries:append(1, packet )

		return proxy.PROXY_SEND_QUERY
	end
end

---
-- read_query_result() is called when we receive a query result 
-- from the server
--
-- inj.query_time is the query-time in micro-seconds
-- 
-- @return 
--   * nothing or proxy.PROXY_SEND_RESULT to pass the result-set to the client
-- 
function read_query_result(inj)
	print("query-time: " .. (inj.query_time / 1000) .. "ms")
	print("response-time: " .. (inj.response_time / 1000) .. "ms")
end
