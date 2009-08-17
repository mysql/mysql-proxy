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

local chassis = assert(require("chassis"))

function read_query(packet)
	if packet:byte() ~= proxy.COM_QUERY then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK
		}
		return proxy.PROXY_SEND_RESULT
	end

	if packet:sub(2) == "SELECT STR_REPEAT(\"x\", 16 * 1024 * 1024)" then
		proxy.queries:append(1, string.char(3) .. "SELECT \""..("x"):rep(16 * 1024 * 1024 - 14 - 2).."\" AS f", { resultset_is_needed = true } )
		return proxy.PROXY_SEND_QUERY
	else
		proxy.queries:append(1, packet, { resultset_is_needed = false } )
		return proxy.PROXY_SEND_QUERY
	end
end

function read_query_result(inj)
	if inj.id == 2 then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = "f", type = proxy.MYSQL_TYPE_STRING },
				},
				rows = { { "foo"  }  }
			}
		}
		return proxy.PROXY_SEND_RESULT
	end
end
