---
-- test if connection pooling works
--
-- by comparing the statement-ids and the connection ids we can 
-- track if the ro-pooling script was reusing a connection
--
function packet_auth(fields)
	fields = fields or { }
	return "\010" ..             -- proto version
		(fields.version or "5.0.45-proxy") .. -- version
		"\000" ..             -- term-null
		"\001\000\000\000" .. -- thread-id
		"\065\065\065\065" ..
		"\065\065\065\065" .. -- challenge - part I
		"\000" ..             -- filler
		"\001\130" ..         -- server cap (long pass, 4.1 proto)
		"\008" ..             -- charset
		"\002\000" ..         -- status
		("\000"):rep(13) ..   -- filler
		"\065\065\065\065"..
		"\065\065\065\065"..
		"\065\065\065\065"..
		"\000"                -- challenge - part II
end

-- will be called once after connect
stmt_id = 0
conn_id = 0

function connect_server()
	-- the first connection inits the global counter
	if not proxy.global.conn_id then
		proxy.global.conn_id = 0
	end
	proxy.global.conn_id = proxy.global.conn_id + 1

	-- set our connection id
	conn_id = proxy.global.conn_id

	-- emulate a server
	proxy.response = {
		type = proxy.MYSQLD_PACKET_RAW,
		packets = {
			packet_auth()
		}
	}
	return proxy.PROXY_SEND_RESULT
end

function read_query(packet)
	if packet:byte() ~= proxy.COM_QUERY then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK
		}
		return proxy.PROXY_SEND_RESULT
	end

	-- query-counter for this connection
	stmt_id = stmt_id + 1

	local query = packet:sub(2) 
	if query == 'SELECT counter' then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = 'conn_id' },
					{ name = 'stmt_id' },
				},
				rows = { { tostring(conn_id), tostring(stmt_id) } }
			}
		}
	else
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
			errmsg = "(pooling-mock) " .. query
		}
	end
	return proxy.PROXY_SEND_RESULT
end




