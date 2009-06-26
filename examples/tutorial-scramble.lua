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

local password = assert(require("mysql.password"))
local proto = assert(require("mysql.proto"))

---
-- map usernames to another login
local map_auth = {
	["replace"] = {
		password = "me",
		new_user = "root",
		new_password = "secret"
	}
}

---
-- show how to use the mysql.password functions
--
function read_auth()
	local c = proxy.connection.client
	local s = proxy.connection.server

	print(("for challenge %q the client sent %q"):format(
		s.scramble_buffer,
		c.scrambled_password
	))

	-- if we know this user, replace its credentials
	local mapped = map_auth[c.username]
	
	if mapped and
		password.check(
			s.scramble_buffer,
			c.scrambled_password,
			password.hash(password.hash(mapped.password))
		) then

		proxy.queries:append(1, 
			proto.to_response_packet({
				username = mapped.new_user,
				response = password.scramble(s.scramble_buffer, password.hash(mapped.new_password)),
				charset  = 8, -- default charset
				database = c.default_db,
				max_packet_size = 1 * 1024 * 1024
			})
		)

		return proxy.PROXY_SEND_QUERY
	end
end

