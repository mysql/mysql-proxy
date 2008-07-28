local tokenizer = require("proxy.tokenizer")
local proto = require("mysql.proto")

function connect_server()
	-- emulate a server
	proxy.response = {
		type = proxy.MYSQLD_PACKET_RAW,
		packets = {
			proto.to_challenge_packet({})
		}
	}
	return proxy.PROXY_SEND_RESULT
end

--
-- returns the tokens of every query
--

-- Uncomment the following line if using with MySQL command line.
-- Keep it commented if using inside the test suite
-- local counter = 0

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
    local tokens = tokenizer.tokenize(query)
    proxy.response.type = proxy.MYSQLD_PACKET_OK
    proxy.response.resultset = {
        fields = {
            { type = proxy.MYSQL_TYPE_STRING, name = "text", },
        },
        rows = { { '<'..tokenizer.normalize(tokens)..'>' } }
    }
    return proxy.PROXY_SEND_RESULT
end
