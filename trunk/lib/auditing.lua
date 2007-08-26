--[[

   Copyright (C) 2007 MySQL AB

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


