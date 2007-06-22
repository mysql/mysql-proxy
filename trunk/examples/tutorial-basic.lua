---
-- read_query() gets the client query before it reaches the server
--
-- @param packet the mysql-packet sent by client
--
-- the packet contains a command-packet:
--  * the first byte the type (e.g. proxy.COM_QUERY)
--  * the argument of the command
--
--   http://forge.mysql.com/wiki/MySQL_Internals_ClientServer_Protocol#Command_Packet
--
-- for a COM_QUERY it is the query itself in plain-text
--
function read_query( packet )
	if string.byte(packet) == proxy.COM_QUERY then
		print("we got a normal query: " .. string.sub(packet, 2))
	end
end

