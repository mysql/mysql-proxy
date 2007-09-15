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

   Written by Giuseppe Maxia, QA Developer

--]]

---
--[[ 
	Simple example of a run-time module loader for MySQL Proxy
	
	Usage: 
	1. load this module in the Proxy.
	
	2. From a client, run the command 
	   PLOAD name_of_script
	   and you can use the features implemented in the new script
	   immediately.
	
	3. Successive calls to PLOAD will unload the previous script and
	   load a new one.

	IMPORTANT NOTICE:
	the proxy will try to load the file in the directory where
	the proxy (NOT THE CLIENT) was started.
	To use modules not in such directory, you need to
	specify an absolute path.
--]]

local VERSION = '0.1.2'

---
-- Handlers for read_query() and read_query_result().
-- Initially, they are void.
-- If the loaded module has implemented read_query() and read_query_result()
-- functions, they are associated with these handlers
--
local read_query_handler = nil
local read_query_result_handler = nil

-- 
-- an error string to describe the error occurring
--
local load_multi_error_str = ''

-- set_error()
--
-- sets the error string 
--
-- @param msg the message to be assigned to the error string
--
function set_error(msg)
	load_multi_error_str = msg
	print (msg)
	return nil
end

---
-- file_exists()
--
-- checks if a file exists
--
-- @param fname the file name
--
function file_exists(fname)
	local fh=io.open( fname, 'r')
	if fh then 
		fh:close()
		return true
	else
		return false
	end
end

---
--  get_module()
--  
--  This function scans an existing lua file, and turns it into
--  a closure exporting two function handlers, one for
--  read_query() and one for read_query_result().
--  If the input script does not contain the above functions,
--  get_module fails.
--
--	@param module_name the name of the existing script
--
function get_module(module_name)
	-- 
	-- assumes success
	--
	load_multi_error_str = ''

	--
	-- the module is copied to a temporary file
	-- on a given directory
	--
	local new_module = 'tmpmodule'
	local tmp_dir	= '/tmp/'
	local new_filename = tmp_dir .. new_module .. '.lua'
	local source_script = module_name
	if not source_script:match('.lua$') then
		source_script = source_script .. '.lua'
	end
	-- 
	-- if the new module does not exist
	-- an error is returned
	--
	if not file_exists(source_script) then
		set_error('file not found ' .. source_script)
		return
	end
	--
	-- if the module directory is not on the search path,
	-- we need to add it
	--
	if not package.path:match(tmp_dir) then
		package.path = tmp_dir .. '?.lua;' .. package.path
	end
	--
	-- Make sure that the module is not loaded.
	-- If we don't remove it from the list of loaded modules,
	-- subsequent load attempts will silently fail
	--
	package.loaded['tmpmodule'] = nil 
	local ofh = io.open(new_filename, 'w')
	--
	-- Writing to the new package, starting with the
	-- header and the wrapper function
	--
	ofh:write( string.format(
				 "module('%s', package.seeall)\n"
			  .. "function make_funcs()\n" , new_module)
	)
	local found_rq = false
	local found_rqr = false
	--
	-- Copying contents from the original script
	-- to the new module, and checking for the existence
	-- of the handler functions
	--
	for line in io.lines(source_script) do
		ofh:write(line .. "\n")
		if line:match('^%s*function%s+read_query%s*%(') then
			found_rq = true
		elseif line:match('^%s*function%s+read_query_result%s*%(') then
			found_rqr = true
		end
	end
	--
	-- closing the wrapper on the new module
	--
	ofh:write(
		   "return { rq = read_query, rqr = read_query_result }\n"
		.. "end\n"
	)
	ofh:close()
	--
	-- Final check. If the handlers were not found, the load fails
	--
	--
	if (found_rq == false ) then
		set_error('script ' .. source_script .. 
			' does not contain a "read_query" function')
		return
	end
	if (found_rqr == false ) then
		set_error('script ' .. source_script .. 
			' does not contain a "read_query_result" function')
		return
	end
	--
	-- All set. The new module is loaded
	-- with a new function that will return the handlers
	--
	local result = require(new_module)
	return result
end

---
-- simple_dataset()
--
-- returns a dataset made of a header and a message
--
-- @param header the column name
-- @param message the contents of the column
function simple_dataset (header, message) 
	proxy.response.type = proxy.MYSQLD_PACKET_OK
	proxy.response.resultset = {
		fields = {{type = proxy.MYSQL_TYPE_STRING, name = header}},
		rows = { { message} }
	}
	return proxy.PROXY_SEND_RESULT
end

---
-- make_regexp_from_command()
--
-- creates a regular expression for fast scanning of the command
-- 
-- @param cmd the command to be converted to regexp
--
function make_regexp_from_command(cmd)
	local regexp= '^%s*';
	for ch in cmd:gmatch('(.)') do
		regexp = regexp .. '[' .. ch:upper() .. ch:lower() .. ']' 
	end
	regexp = regexp  .. '%s+(%S+)'
	return regexp
end

-- 
-- The default command for loading a new module is PLOAD
-- You may change it through an environment variable
--
local proxy_load_command = os.getenv('PLOAD') ;
local pload_regexp = '^%s*[Pp][Ll][Oo][Aa][Dd]%s+(%S+)'

if proxy_load_command then
	pload_regexp = make_regexp_from_command(proxy_load_command)
end

--
-- This function is called at each query.
-- The request for loading a new script is handled here
--
function read_query (packet)
	if packet:byte() ~= proxy.COM_QUERY then
		return
	end
	local query = packet:sub(2)
	-- Checks if a PLOAD command was issued.
	-- A regular expresion check is faster than 
	-- doing a full tokenization. (Especially on large queries)
	--
	local new_module = query:match(pload_regexp)
	if (new_module) then
		--[[
		   If a request for loading is received, then
		   we attempt to load the new module using the
		   get_module() function
		--]]
		local new_tablespace = get_module(new_module)
		if (new_tablespace) then
			local handlers = new_tablespace.make_funcs()
			--
			-- The new module must have two handlers for read_query()
			-- and read_query_result(). The loading function
			-- returns nil if such handlers don't exist
			--
			if handlers['rq'] then
				read_query_handler = handlers['rq']
			else
				return simple_dataset('ERROR',
					'could not load handle for read_query')
			end
			if handlers['rqr'] then
				read_query_result_handler = handlers['rqr']
			else
				return simple_dataset('ERROR',
					'could not load handle for read_query_result')
			end
			-- 
			-- Returns a confirmation that a new  module was loaded
			--
			return simple_dataset('info', 'module "' .. new_module .. '" loaded' )
		else
			--
			-- The load was not successful.
			-- Inform the user
			--
			return simple_dataset('ERROR', 'load of "' 
				.. new_module .. '" failed (' 
				.. load_multi_error_str .. ')' )
		end
	end
	--
	-- If a handler was installed from a new module, it is called
	-- now. 
	--
	if read_query_handler then
		return read_query_handler(packet)
	end
end

--
-- this function is called every time a result set is
-- returned after an injected query
--
function read_query_result(inj)
	-- 
	-- If the dynamically loaded module had an handler for read_query_result()
	-- it is called now
	--
	if read_query_result_handler then
		return read_query_result_handler(inj)
	end
end

