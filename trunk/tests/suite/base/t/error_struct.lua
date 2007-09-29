
function read_query( packet ) 
	if packet:byte() ~= proxy.COM_QUERY then 
		return 
	end
    if packet:sub(2):lower():match('^%s*select%s+error') then
		local errno = 3306
		local sql_state = 'S12XYZ'
		proxy.response = { 
			type     = proxy.MYSQLD_PACKET_ERR, 
			errmsg   = 'FAKE ERROR MESSAGE', 
			errcode  = errno, 
			sqlstate = sql_state
		}
		return proxy.PROXY_SEND_RESULT
	end
end
