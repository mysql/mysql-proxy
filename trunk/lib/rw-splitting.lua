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

---
-- a flexible statement based load balancer with connection pooling
--
-- * build a connection pool of min_idle_connections for each backend and maintain
--   its size
-- * 
-- 
-- 

local commands  = require("proxy.commands")
local tokenizer = require("proxy.tokenizer")
local lb        = require("proxy.balance")

--- config
--
-- connection pool
proxy.global.config.min_idle_connections = 4
proxy.global.config.max_idle_connections = 8

-- debug
proxy.global.config.is_debug = true

--- end of config

---
-- read/write splitting sends all non-transactional SELECTs to the slaves
--
-- is_in_transaction tracks the state of the transactions
local is_in_transaction       = false

-- if this was a SELECT SQL_CALC_FOUND_ROWS ... stay on the same connections
local is_in_select_calc_found_rows = false

--- 
-- get a connection to a backend
--
-- as long as we don't have enough connections in the pool, create new connections
--
function connect_server() 
	local is_debug = proxy.global.config.is_debug
	-- make sure that we connect to each backend at least ones to 
	-- keep the connections to the servers alive
	--
	-- on read_query we can switch the backends again to another backend

	if is_debug then
		print()
		print("[connect_server] ")
	end

	local least_idle_conns_ndx = 0
	local least_idle_conns = 0

	for i = 1, #proxy.backends do
		local s = proxy.backends[i]
		if is_debug then
			print("  [".. i .."].connected_clients = " .. s.connected_clients)
			print("  [".. i .."].idling_connections = " .. s.idling_connections)
			print("  [".. i .."].type = " .. s.type)
			print("  [".. i .."].state = " .. s.state)
		end

		if s.state ~= proxy.BACKEND_STATE_DOWN then
			-- try to connect to each backend once at least
			if s.idling_connections == 0 then
				proxy.connection.backend_ndx = i
				print("  [".. i .."] open new connection")
				return
			end

			-- try to open at least min_idle_connections
			if least_idle_conns_ndx == 0 or
			   ( s.idling_connections < proxy.global.config.min_idle_connections and 
			     s.idling_connections < least_idle_conns ) then
				least_idle_conns_ndx = i
				least_idle_conns = s.idling_connections
			end
		end
	end

	if least_idle_conns_ndx > 0 then
		proxy.connection.backend_ndx = least_idle_conns_ndx
	end

	if proxy.connection.backend_ndx > 0 and 
	   proxy.backends[proxy.connection.backend_ndx].idling_connections >= proxy.global.config.min_idle_connections then
		-- we have 4 idling connections in the pool, that's good enough
		if is_debug then
			print("  using pooled connection from: " .. proxy.connection.backend_ndx)
		end
	
		return proxy.PROXY_IGNORE_RESULT
	end

	if is_debug then
		print("  opening new connection on: " .. proxy.connection.backend_ndx)
	end

	-- open a new connection 
end

--- 
-- put the successfully authed connection into the connection pool
--
-- @param auth the context information for the auth
--
-- auth.packet is the packet
function read_auth_result( auth )
	if auth.packet:byte() == proxy.MYSQLD_PACKET_OK then
		-- auth was fine, disconnect from the server
		proxy.connection.backend_ndx = 0
	elseif auth.packet:byte() == proxy.MYSQLD_PACKET_EOF then
		-- we received either a 
		-- 
		-- * MYSQLD_PACKET_ERR and the auth failed or
		-- * MYSQLD_PACKET_EOF which means a OLD PASSWORD (4.0) was sent
		print("(read_auth_result) ... not ok yet");
	elseif auth.packet:byte() == proxy.MYSQLD_PACKET_ERR then
		-- auth failed
	end
end


--- 
-- read/write splitting
function read_query( packet )
	local is_debug = proxy.global.config.is_debug
	local cmd      = commands.parse(packet)
	local c        = proxy.connection.client

	local tokens
	local norm_query

	-- handle script-options first
	if cmd.type == proxy.COM_QUERY and 
	   cmd.query:sub(1, 3):upper() == "SET" and 
	   #cmd.query < 32 then
		tokens     = tokens or assert(tokenizer.tokenize(cmd.query))
		norm_query = norm_query or tokenizer.normalize(tokens)
	
		if norm_query == "SET `GLOBAL` `rwsplit` . `debug` = ? " then
			if tokens[#tokens].token_name == "TK_INTEGER" then
				proxy.global.config.is_debug = (tokens[#tokens].text ~= "0")

				proxy.response = {
					type = proxy.MYSQLD_PACKET_OK,
				}
			else
				proxy.response = {
					type = proxy.MYSQLD_PACKET_ERR,
					errmsg = "SET GLOBAL rwsplit.debug = <int>, got " .. tokens[#tokens].token_name
				}
			end
			if is_debug then
				print("set rwsplit.debug: " .. tostring(proxy.global.config.is_debug))
			end
			return proxy.PROXY_SEND_RESULT
		end
	end

	-- looks like we have to forward this statement to a backend
	if is_debug then
		print("[read_query]")
		print("  current backend   = " .. proxy.connection.backend_ndx)
		print("  client default db = " .. c.default_db)
		print("  client username   = " .. c.username)
		if cmd.type == proxy.COM_QUERY then 
			print("  query             = "        .. cmd.query)
		end
	end

	if cmd.type == proxy.COM_QUIT then
		-- don't send COM_QUIT to the backend. We manage the connection
		-- in all aspects.
		proxy.response = {
			type = proxy.MYSQLD_PACKET_OK,
		}

		return proxy.PROXY_SEND_RESULT
	end

	proxy.queries:append(1, packet)

	-- read/write splitting 
	--
	-- send all non-transactional SELECTs to a slave
	if not is_in_transaction and
	   cmd.type == proxy.COM_QUERY then
		tokens     = tokens or assert(tokenizer.tokenize(cmd.query))

		local stmt = tokenizer.first_stmt_token(tokens)
		print("stmt.token_name: " .. stmt.token_name)

		if stmt.token_name == "TK_SQL_SELECT" then
			is_in_select_calc_found_rows = false

			for i, token in ipairs(tokens) do
				if token.type_name == "TK_SQL_SQL_CALC_FOUND_ROWS" then
					is_in_select_calc_found_rows = true

					break
				end

				-- if we haven't found the special tokens in the first 8
				-- give up
				if i > 8 then
					break
				end
			end

			local backend_ndx = lb.idle_ro()

			print("ro-backend: " .. backend_ndx)

			if backend_ndx > 0 then
				proxy.connection.backend_ndx = backend_ndx
			end
		end
	end

	print("current backend: " .. proxy.connection.backend_ndx)

	-- no backend selected yet, pick a master
	if proxy.connection.backend_ndx == 0 then
		-- we don't have a backend right now
		-- 
		-- let's pick a master as a good default
		--
		proxy.connection.backend_ndx = lb.idle_failsafe_rw()
		print("rw-backend: " .. proxy.connection.backend_ndx)
	end

	-- by now we should have a backend
	--
	-- in case the master is down, we have to close the client connections
	-- otherwise we can go on
	if proxy.connection.backend_ndx == 0 then
		return proxy.PROXY_SEND_QUERY
	end

	local s = proxy.connection.server

	-- if client and server db don't match, adjust the server-side 
	--
	-- skip it if we send a INIT_DB anyway
	if cmd.type ~= proxy.COM_INIT_DB and 
	   c.default_db and c.default_db ~= s.default_db then
		print("    server default db: " .. s.default_db)
		print("    client default db: " .. c.default_db)
		print("    syncronizing")
		proxy.queries:prepend(2, string.char(proxy.COM_INIT_DB) .. c.default_db)
	end

	-- send to master
	if is_debug then
		if proxy.connection.backend_ndx > 0 then
			local b = proxy.backends[proxy.connection.backend_ndx]
			print("  sending to backend : " .. b.address);
			print("    is_slave         : " .. tostring(b.type == proxy.BACKEND_TYPE_RO));
			print("    server default db: " .. s.default_db)
			print("    server username  : " .. s.username)
		end
		print("    in_trans        : " .. tostring(is_in_transaction));
		print("    in_calc_found   : " .. tostring(is_in_select_calc_found_rows));
		print("    COM_QUERY       : " .. tostring(cmd.type == proxy.COM_QUERY));
	end

	return proxy.PROXY_SEND_QUERY
end

---
-- as long as we are in a transaction keep the connection
-- otherwise release it so another client can use it
function read_query_result( inj ) 
	local res      = assert(inj.resultset)
  	local flags    = res.flags

	if inj.id ~= 1 then
		-- ignore the result of the USE <default_db>
		-- the DB might not exist on the backend, what do do ?
		--
		if inj.id == 2 then
			-- the injected INIT_DB failed as the slave doesn't have this DB
			-- or doesn't have permissions to read from it
			if res.query_status == proxy.MYSQLD_PACKET_ERR then
				proxy.queries:reset()

				proxy.response = {
					type = proxy.MYSQLD_PACKET_ERR,
					errmsg = "can't change DB ".. proxy.connection.client.default_db ..
						" to on slave " .. proxy.backends[proxy.connection.backend_ndx].address
				}

				return proxy.PROXY_SEND_RESULT
			end
		end
		return proxy.PROXY_IGNORE_RESULT
	end
	is_in_transaction = flags.in_trans

	if not is_in_transaction and 
	   not is_in_select_calc_found_rows then
		-- release the backend
		proxy.connection.backend_ndx = 0
	end
end

--- 
-- close the connections if we have enough connections in the pool
--
-- @return nil - close connection 
--         IGNORE_RESULT - store connection in the pool
function disconnect_client()
	local is_debug = proxy.global.config.is_debug
	if is_debug then
		print("[disconnect_client]")
	end

	if proxy.connection.backend_ndx == 0 then
		-- currently we don't have a server backend assigned
		--
		-- pick a server which has too many idling connections and close one
		for i = 1, #proxy.backends do
			local s = proxy.backends[i]

			if s.state ~= proxy.BACKEND_STATE_DOWN and
			   s.idling_connections > proxy.global.config.max_idle_connections then
				-- try to disconnect a backend
				proxy.connection.backend_ndx = i
				if is_debug then
					print("  [".. i .."] closing connection, idling: " .. proxy.backends[i].idling_connections)
				end
				return
			end
		end
	end
end

