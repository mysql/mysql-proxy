--[[

   Copyright (C) 2007, 2008 MySQL AB

--]]

local user = ""

function read_handshake( auth )
	print("<-- let's send him some information about us")
	print("    server-addr   : " .. auth.server_addr)
	print("    client-addr   : " .. auth.client_addr)
end

function read_auth( auth )
	print("--> there, look, the client is responding to the server auth packet")
	print("    username      : " .. auth.username)

	user = auth.username
end

function read_auth_result( auth )
	local state = auth.packet:byte()

	if state == proxy.MYSQLD_PACKET_OK then
		print("<-- auth ok");
	elseif state == proxy.MYSQLD_PACKET_ERR then
		print("<-- auth failed");
	else
		print("<-- auth ... don't know: " .. string.format("%q", auth.packet));
	end
end

function read_query( packet ) 
	print("--> '".. user .."' sent us a query")
	if packet:byte() == proxy.COM_QUERY then
		print("    query: " .. packet:sub(2))
	end
end


