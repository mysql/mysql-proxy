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

res = { }

function read_query(packet)
	if packet:byte() ~= proxy.COM_QUERY then return end

	local q = packet:sub(2)

	res = { }

	if q:sub(1, 6):upper() == "SELECT" then
		proxy.queries:append(1, packet)
		proxy.queries:append(2, packet)

		return proxy.PROXY_SEND_QUERY
	end
end

function read_query_result(inj)
	for row in inj.resultset.rows do
		res[#res + 1] = row
	end

	if inj.id ~= 2 then
		return proxy.PROXY_IGNORE_RESULT
	end

	proxy.response = {
		type = proxy.MYSQLD_PACKET_OK,
		resultset = {
			rows = res
		}
	}

	local fields = {} 
	for n = 1, #inj.resultset.fields do
		fields[#fields + 1] = { 
			type = inj.resultset.fields[n].type,
			name = inj.resultset.fields[n].name,
		}
	end

	proxy.response.resultset.fields = fields

	return proxy.PROXY_SEND_RESULT
end


