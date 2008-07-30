
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

function connect_server()
	-- emulate a server
	proxy.response = {
		type = proxy.MYSQLD_PACKET_RAW,
		packets = {
			packet_auth()
		}
	}
	return proxy.PROXY_SEND_RESULT
end

-- reply with a single field and row containing an indication if we resolved the client's address.
-- it should not be NULL!
function read_query( packet )

    if packet:byte() ~= proxy.COM_QUERY then
        proxy.response = { type = proxy.MYSQLD_PACKET_OK } 
        return proxy.PROXY_SEND_RESULT
    end
    local query = packet:sub(2)
    --
    -- Uncomment the following two lines if using with MySQL command line.
    -- Keep them commented if using inside the test suite
    -- counter = counter + 1
    -- if counter < 3 then return end
	local client_addr_check
	if proxy.connection.client.address ~= nil then
		client_addr_check = "not-nil"
	else
		client_addr_check = "nil"
	end
    proxy.response.type = proxy.MYSQLD_PACKET_OK
    proxy.response.resultset = {
        fields = {
            { type = proxy.MYSQL_TYPE_STRING, name = "client", },
        },
        rows = { { client_addr_check }  }
    }
    return proxy.PROXY_SEND_RESULT
end
