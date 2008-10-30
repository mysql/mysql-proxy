--[[ $%BEGINLICENSE%$
 Copyright (C) 2007-2008 MySQL AB, 2008 Sun Microsystems, Inc

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 $%ENDLICENSE%$ --]]
--[[

   

--]]

function connect_server() 
	print("--> a client really wants to talk to a server")
end

function read_handshake( auth )
	print("<-- let's send him some information about us")
	print("    mysqld-version: " .. auth.mysqld_version)
	print("    thread-id     : " .. auth.thread_id)
	print("    scramble-buf  : " .. string.format("%q", auth.scramble))
	print("    server-addr   : " .. auth.server_addr)
	print("    client-addr   : " .. auth.client_addr)

	-- lets deny clients from !127.0.0.1
	if not auth.client_addr:match("^127.0.0.1:") then
		proxy.response.type = proxy.MYSQLD_PACKET_ERR
		proxy.response.errmsg = "only local connects are allowed"

		print("we don't like this client");

		return proxy.PROXY_SEND_RESULT
	end
end

function read_auth( auth )
	print("--> there, look, the client is responding to the server auth packet")
	print("    username      : " .. auth.username)
	print("    password      : " .. string.format("%q", auth.password))
	print("    default_db    : " .. auth.default_db)

	if auth.username == "evil" then
		proxy.response.type = proxy.MYSQLD_PACKET_ERR
		proxy.response.errmsg = "evil logins are not allowed"
		
		return proxy.PROXY_SEND_RESULT
	end
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
	print("--> someone sent us a query")
	if packet:byte() == proxy.COM_QUERY then
		print("    query: " .. packet:sub(2))

		if packet:sub(2) == "SELECT 1" then
			proxy.queries:append(1, packet)
		end
	end

end

function read_query_result( inj ) 
	print("<-- ... ok, this only gets called when read_query() told us")

	proxy.response = {
		type = proxy.MYSQLD_PACKET_RAW,
		packets = { 
			"\255" ..
			  "\255\004" .. -- errno
			  "#" ..
			  "12S23" ..
			  "raw, raw, raw"
		}
	}

	return proxy.PROXY_SEND_RESULT
end
