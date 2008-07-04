---
-- map SQL commands to the hidden MySQL Protocol commands
--
-- some protocol commands are only available through the mysqladmin tool like
-- * ping
-- * shutdown
-- * debug
-- * statistics
--
-- ... while others are avaible
-- * process info (SHOW PROCESS LIST)
-- * process kill (KILL <id>)
-- 
-- ... and others are ignored
-- * time
-- 

function read_query(packet)
	if packet:byte() ~= proxy.COM_QUERY then return end

	if packet:sub(2) == "COMMIT SUICIDE" then
		proxy.queries:append(proxy.COM_SHUTDOWN, string.char(proxy.COM_SHUTDOWN))
		return proxy.PROXY_SEND_QUERY
	elseif packet:sub(2) == "PING" then
		proxy.queries:append(proxy.COM_PING, string.char(proxy.COM_PING))
		return proxy.PROXY_SEND_QUERY
	elseif packet:sub(2) == "STATISTICS" then
		proxy.queries:append(proxy.COM_STATISTICS, string.char(proxy.COM_STATISTICS))
		return proxy.PROXY_SEND_QUERY
	elseif packet:sub(2) == "PROCINFO" then
		proxy.queries:append(proxy.COM_PROCESS_INFO, string.char(proxy.COM_PROCESS_INFO))
		return proxy.PROXY_SEND_QUERY
	elseif packet:sub(2) == "TIME" then
		proxy.queries:append(proxy.COM_TIME, string.char(proxy.COM_TIME))
		return proxy.PROXY_SEND_QUERY
	elseif packet:sub(2) == "DEBUG" then
		proxy.queries:append(proxy.COM_DEBUG, string.char(proxy.COM_DEBUG))
		return proxy.PROXY_SEND_QUERY
	elseif packet:sub(2) == "PROCKILL" then
		proxy.queries:append(proxy.COM_PROCESS_KILL, string.char(proxy.COM_PROCESS_KILL))
		return proxy.PROXY_SEND_QUERY
	elseif packet:sub(2) == "SETOPT" then
		proxy.queries:append(proxy.COM_SET_OPTION, string.char(proxy.COM_SET_OPTION))
		return proxy.PROXY_SEND_QUERY
	end
end

function read_query_result(inj)

	if inj.id == proxy.COM_SHUTDOWN or
	   inj.id == proxy.COM_SET_OPTION or
	   inj.id == proxy.COM_DEBUG then
		-- translate the EOF packet from the COM_SHUTDOWN into a OK packet
		-- to match the needs of the COM_QUERY we got
		if inj.resultset.raw:byte() ~= 255 then
			proxy.response = {
				type = proxy.MYSQLD_PACKET_OK,
			}
			return proxy.PROXY_SEND_RESULT
		end
	elseif inj.id == proxy.COM_PING or
	       inj.id == proxy.COM_TIME or
	       inj.id == proxy.COM_PROCESS_KILL or
	       inj.id == proxy.COM_PROCESS_INFO then
		-- no change needed
	elseif inj.id == proxy.COM_STATISTICS then
		-- the response a human readable plain-text
		--
		-- just turn it into a proper result-set
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = "statisitics" }
				},
				rows = {
					{ inj.resultset.raw }
				}
			}
		}
		return proxy.PROXY_SEND_RESULT

	else
		print(("got: %q"):format(inj.resultset.raw))
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
		}
		return proxy.PROXY_SEND_RESULT
	end
end
