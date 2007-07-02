local affected_rows

function read_query(packet)
	if packet:byte() == proxy.COM_QUERY then
		if packet:sub(2) == "SELECT affected_rows" then
			proxy.response.type = proxy.MYSQLD_PACKET_OK
			proxy.response.resultset = {
				fields = { 
					{ name = "rows", type = proxy.MYSQL_TYPE_LONG }
				},
				rows = {
					{ affected_rows }
				}
			}
			
			return proxy.PROXY_SEND_RESULT
		else
			proxy.queries:append(1, packet)
		end
	end
end

function read_query_result(inj) 
	affected_rows = inj.resultset.affected_rows
end

