--[[ $%BEGINLICENSE%$
 Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; version 2 of the
 License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA

 $%ENDLICENSE%$ --]]

---
-- forward the query AS IS and add a query to the queue,
-- but don't mark it as "SEND_QUERY"
--
-- the query in the queue will never be touched, as the default behaviour
-- is to just forward the result back to the client without buffering 
-- it
function read_query( packet )
	if packet:byte() == proxy.COM_QUERY then
		proxy.queries:append(1, packet, { resultset_is_needed = true } )
	end

	-- forward the incoming query AS IS
end

---
-- try access the resultset 
-- 
function read_query_result(inj)
	local res = assert(inj.resultset)

	if res.query_status == proxy.MYSQLD_PACKET_ERR then
		print(("received error-code: %d"):format(
			res.raw:byte(2)+(res.raw:byte(3)*256)
		))
	end
end
