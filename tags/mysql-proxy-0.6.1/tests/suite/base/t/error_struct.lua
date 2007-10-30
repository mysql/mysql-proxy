
function read_query( packet ) 
	if packet:byte() ~= proxy.COM_QUERY then 
		return 
	end
    local err_code,sql_state, err_msg = 
        packet:sub(2):lower():match('^%s*select%s+error%s+(%d+)%s+(%w+)%s+"(.*)"') 
    if (err_code and sql_state) then
		proxy.response = { 
			type     = proxy.MYSQLD_PACKET_ERR, 
			errmsg   = err_msg,
			errcode  = err_code, 
			sqlstate = sql_state
		}
		return proxy.PROXY_SEND_RESULT
	end
end
