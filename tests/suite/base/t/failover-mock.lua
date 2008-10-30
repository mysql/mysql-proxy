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
-- test if failover works
--
-- * this script is started twice to simulate two backends
-- * one is shutdown in the test with COM_SHUTDOWN
--

require("chassis")

function packet_auth(fields)
	fields = fields or { }
	return "\010" ..             -- proto version
		(fields.version or "5.0.45-proxy") .. -- version
		"\000" ..             -- term-null
		"\001\000\000\000" .. -- thread-id
		"\065\065\065\065" ..
		"\065\065\065\065" .. -- challenge - part I
		"\000" ..             -- filler
		"\001\130" ..         -- server cap (long pass, 4.1 proto)
		"\008" ..             -- charset
		"\002\000" ..         -- status
		("\000"):rep(13) ..   -- filler
		"\065\065\065\065"..
		"\065\065\065\065"..
		"\065\065\065\065"..
		"\000"                -- challenge - part II
end

function connect_server()
	-- emulate a server
	proxy.response = {
		type = proxy.MYSQLD_PACKET_RAW,
		packets = {
			packet_auth()
		}
	}
	return proxy.PROXY_SEND_RESULT
end

if not proxy.global.backend_id then
	proxy.global.backend_id = 0
end

---
-- 
function read_query(packet)
	if packet:byte() == proxy.COM_SHUTDOWN then
		-- stop the proxy if we are asked to
		chassis.set_shutdown()
		proxy.response = {
			type = proxy.MYSQLD_PACKET_RAW,
			packets = { string.char(254) },
		}
		return proxy.PROXY_SEND_RESULT
	elseif packet:byte() ~= proxy.COM_QUERY then
		-- just ACK all non COM_QUERY's
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK
		}
		return proxy.PROXY_SEND_RESULT
	end

	local query = packet:sub(2) 
	local set_id = query:match('SET ID (.)')

	if query == 'GET ID' then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = 'id' },
				},
				rows = { { proxy.global.backend_id } }
			}
		}
	elseif set_id then
		proxy.global.backend_id = set_id
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
			resultset = {
				fields = {
					{ name = 'id' },
				},
				rows = { { proxy.global.backend_id } }
			}
		}

	else
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
			errmsg = "(pooling-mock) " .. query
		}
	end
	return proxy.PROXY_SEND_RESULT
end




