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

---
-- show how to use the mysql.password functions 
function read_auth()
	local c = proxy.connection.client
	local s = proxy.connection.server
	print(("for challenge %q the client sent %q"):format(
		s.scramble_buffer,
		c.scrambled_password
	))

	local cleartext = "123"
	local hashed_password = password.hash(cleartext) -- same as sha1(cleartext)
	local response  = password.scramble(s.scramble_buffer, hashed_password)

	print(("for challenge %q and password %q we would send %q"):format(
		s.scramble_buffer,
		cleartext,
		response
	))

	-- your password is '123' the 'response' should match 'c.scrambled_password'
end
