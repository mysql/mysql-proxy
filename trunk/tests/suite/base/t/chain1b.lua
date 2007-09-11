
function read_query(packet)
    if packet:byte() ~= proxy.COM_QUERY then
        return
    end
    local query = packet:sub(2) 
    if query == 'SELECT 1' then
        proxy.queries:append(1, string.char(proxy.COM_QUERY) .. 'SELECT 1, "ADDITION"')
        return proxy.PROXY_SEND_QUERY
    end
end

function read_query_result(inj)

end



