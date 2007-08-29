local tk = require('proxy.tokenizer')

function read_query( packet )
    if packet:byte() ~= proxy.COM_QUERY then
        return
    end
    local query = packet:sub(2)
    local tokens = tk.tokenize(query)
    local simple_tokens = tk.bare_tokens(tokens, true )
    proxy.response.type = proxy.MYSQLD_PACKET_OK
    proxy.response.resultset = {
        fields = {
            { type = proxy.MYSQL_TYPE_STRING, name = "item", },
            { type = proxy.MYSQL_TYPE_STRING, name = "value", },
        },
        rows = {
            { 'original', query },
            { 'rebuilt' , tk.tokens_to_query(tokens) }
        }
    }
    return proxy.PROXY_SEND_RESULT
end
