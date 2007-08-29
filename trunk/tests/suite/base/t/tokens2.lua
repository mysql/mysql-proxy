--
-- returns the tokens of every query
--

-- Uncomment the following line if using with MySQL command line.
-- Keep it commented if using inside the test suite
-- local counter = 0

function read_query( packet )
    if packet:byte() ~= proxy.COM_QUERY then
        return
    end
    local query = packet:sub(2)
    --
    -- Uncomment the following two lines if using with MySQL command line.
    -- Keep them commented if using inside the test suite
    -- counter = counter + 1
    -- if counter < 3 then return end
    local tokens = proxy.tokenize(query)
    proxy.response.type = proxy.MYSQLD_PACKET_OK
    proxy.response.resultset = {
        fields = {
            { type = proxy.MYSQL_TYPE_STRING, name = "id", },
            { type = proxy.MYSQL_TYPE_STRING, name = "name", },
            { type = proxy.MYSQL_TYPE_STRING, name = "text", },
        },
        rows = {  }
    }
    for i,token in pairs(tokens) do
        table.insert(proxy.response.resultset.rows,
            {
               token['token_id'], 
               token['token_name'], 
               token['text'] 
            }
        )
    end
    return proxy.PROXY_SEND_RESULT
end
