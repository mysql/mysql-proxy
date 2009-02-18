--[[ $%BEGINLICENSE%$
 Copyright (C) 2008 MySQL AB, 2008 Sun Microsystems, Inc

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

---
-- reply with a single field and row containing an indication if we resolved the client's address.
-- 
-- some fields are not preditable, so we only say "nil" or "not nil"
function read_query( packet )
	if packet:byte() ~= proxy.COM_QUERY then
		proxy.response = { type = proxy.MYSQLD_PACKET_OK } 
		return proxy.PROXY_SEND_RESULT
	end

	local query = packet:sub(2)
	local value

	if query == "SELECT proxy.connection.client.dst.address" then
		value = proxy.connection.client.dst.address
	elseif query == "SELECT proxy.connection.client.dst.type" then
		value = proxy.connection.client.dst.type
	elseif query == "SELECT proxy.connection.client.dst.name" then
		value = proxy.connection.client.dst.name
	elseif query == "SELECT proxy.connection.client.dst.port" then
		value = proxy.connection.client.dst.port
	elseif query == "SELECT proxy.connection.client.src.address" then
		value = proxy.connection.client.src.address
	elseif query == "SELECT proxy.connection.client.src.type" then
		value = proxy.connection.client.src.type
	elseif query == "SELECT proxy.connection.client.src.name" then
		value = proxy.connection.client.src.name and "not-nil" or "nil"
	elseif query == "SELECT proxy.connection.client.src.port" then
		value = proxy.connection.client.src.port and "not-nil" or "nil"
	elseif query == "SELECT proxy.connection.server.dst.address" then
		value = proxy.connection.server.dst.address
	elseif query == "SELECT proxy.connection.server.dst.type" then
		value = proxy.connection.server.dst.type
	elseif query == "SELECT proxy.connection.server.dst.name" then
		value = proxy.connection.server.dst.name
	elseif query == "SELECT proxy.connection.server.dst.port" then
		value = proxy.connection.server.dst.port
	elseif query == "SELECT proxy.connection.server.src.address" then
		value = proxy.connection.server.src.address
	elseif query == "SELECT proxy.connection.server.src.type" then
		value = proxy.connection.server.src.type
	elseif query == "SELECT proxy.connection.server.src.name" then
		value = proxy.connection.server.src.name and "not-nil" or "nil"
	elseif query == "SELECT proxy.connection.server.src.port" then
		value = proxy.connection.server.src.port and "not-nil" or "nil"
	else
		value = query .. " ... not known"
	end

	proxy.response = {
		type = proxy.MYSQLD_PACKET_OK,
		resultset = {
			fields = {
				{ type = proxy.MYSQL_TYPE_STRING, name = "value", },
			},
			rows = { { value } }
		}
	}
	return proxy.PROXY_SEND_RESULT
end

