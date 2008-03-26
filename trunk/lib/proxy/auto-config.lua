--[[

   Copyright (C) 2007, 2008 MySQL AB

--]]
 
---
-- automatic configure of modules
--
-- SHOW CONFIG

module("proxy.auto-config", package.seeall)

local tokenizer = require("proxy.tokenizer")

local function parse_value(fld, token)
	local t = type(fld)
	local errmsg

	if t == "boolean" then
		if token.token_name == "TK_INTEGER" then
			return (token.text ~= "0")
		elseif token.token_name == "TK_SQL_FALSE" then
			return false
		elseif token.token_name == "TK_SQL_TRUE" then
			return true
		else
			return nil, "(auto-config) expected a boolean, got " .. token.token_name
		end
	elseif t == "number" then
		if token.token_name == "TK_INTEGER" then
			return tonumber(token.text)
		else
			return nil, "(auto-config) expected a number, got " .. token.token_name
		end
	elseif t == "string" then
		if token.token_name == "TK_STRING" then
			return tostring(token.text)
		else
			return nil, "(auto-config) expected a string, got " .. token.token_name
		end
	else
		return nil, "(auto-config) type: " .. t .. " isn't handled yet" 
	end
	
	return nil, "(auto-config) should not be reached"
end

function handle(tbl, cmd)
	---
	-- support old, deprecated API:
	--
	--   auto_config.handle(cmd)
	--
	-- and map it to
	--
	--   proxy.global.config:handle(cmd)
	if cmd == nil and tbl.type and type(tbl.type) == "number" then
		cmd = tbl
		tbl = proxy.global.config
	end
	
	-- handle script-options first
	if cmd.type ~= proxy.COM_QUERY then return nil end

	-- don't try to tokenize log SQL queries
	if #cmd.query > 128 then return nil end
	
	local tokens     = assert(tokenizer.tokenize(cmd.query))
	local is_error = false
	local error_is_fatal = false
	local error_msg = nil
	local cmd_set_module = nil
	local cmd_set_option = nil

	local commands = {
		{ -- PROXY SET ... 
			{ type = "TK_LITERAL", func = function (tk) 
				local is_proxy = (tk.text:upper() == "PROXY")

				return is_proxy
			end },
			{ type = "TK_SQL_SET" },
			{ type = "TK_LITERAL", func = function (tk) 
				error_is_fatal = true

				return tk.text:upper() == "GLOBAL"
			end },
			{ type = "TK_LITERAL", func = function (tk) 
				if type(tbl[tk.text]) ~= "table" then
					return false, ("SET failed, proxy.global.config.%s isn't known"):format(tk.text)
				end

				cmd_set_module = tk.text
				return true
			end },
			{ type = "TK_DOT" },
			{ type = "TK_LITERAL", func = function (tk) 
				if tbl[cmd_set_module][tk.text] == nil then
					return false, ("SET failed, proxy.global.config.%s.%s isn't known"):format(
						cmd_set_module,
						tk.text)
				end
				cmd_set_option = tk.text
				return true
			end },
			{ type = "TK_EQ" },
			{ func = function (tk) 
				-- do the assignment
				local val, errmsg = parse_value(tbl[cmd_set_module][cmd_set_option], tk)

				if val == nil then
					return false, errmsg
				end
				tbl[cmd_set_module][cmd_set_option] = val

				return true
			end },
		},
		{ -- PROXY SHOW CONFIG
			{ type = "TK_LITERAL", func = function (tk)
				return tk.text:upper() == "PROXY"
			end },
			{ type = "TK_SQL_SHOW", func = function(tk) 
				error_is_fatal = true
				return true
			end },
			{ type = "TK_LITERAL", func = function (tk) 
				if tk.text:upper() ~= "CONFIG" then
					return false
				end

				local rows = { }

				for mod, options in pairs(tbl) do
					for option, val in pairs(options) do
						rows[#rows + 1] = { mod, option, tostring(val), type(val) }
					end
				end

				proxy.response = {
					type = proxy.MYSQLD_PACKET_OK,
					resultset = {
						fields = {
							{ name = "module", type = proxy.MYSQL_TYPE_STRING },
							{ name = "option", type = proxy.MYSQL_TYPE_STRING },
							{ name = "value", type = proxy.MYSQL_TYPE_STRING },
							{ name = "type", type = proxy.MYSQL_TYPE_STRING },
						},
						rows = rows
					}
				}
					
				return true
			end },
		},
		{ -- PROXY SAVE CONFIG INTO "<filename>"
			{ type = "TK_LITERAL", func = function (tk)
				return tk.text:upper() == "PROXY"
			end },
			{ type = "TK_LITERAL", func = function (tk)
				return tk.text:upper() == "SAVE"
			end },
			{ type = "TK_LITERAL", func = function (tk) 
				error_is_fatal = true

				return tk.text:upper() == "CONFIG" 
			end },
			{ type = "TK_SQL_INTO" },
			{ type = "TK_STRING", func = function (tk)
				-- save the config into this filename
				local filename = tk.text

				return tbl:save(filename)
			end },
		},
		["PROXY LOAD CONFIG"] = { -- PROXY LOAD CONFIG FROM "<filename>"
			{ type = "TK_LITERAL", func = function (tk)
				print(error_is_fatal)
				return tk.text:upper() == "PROXY"
			end },
			{ type = "TK_SQL_LOAD" },
			{ type = "TK_LITERAL", func = function (tk) 
				error_is_fatal = true

				return tk.text:upper() == "CONFIG" 
			end },
			{ type = "TK_SQL_FROM" },
			{ type = "TK_STRING", func = function (tk)
				-- save the config into this filename
				local filename = tk.text

				return tbl:load(filename)
			end },
		},

	}

	for cmd_key, sql_command in pairs(commands) do
		is_error = false
		error_is_fatal = false
		error_msg = nil

		for tk_key, cmd_token in ipairs(sql_command) do
			local sql_token = tokens[tk_key]

			if not sql_token then
				-- not enough tokens in the cmdline
				is_error = true
				error_msg = ("command too short: %s"):format(cmd.query)
				break
			end

			if cmd_token.type and cmd_token.type ~= sql_token.token_name then
				-- the tk-type is enforced
				is_error = true
				error_msg = ("[%s].token[%d] types didn't matched: %s != %s (%s)"):format(
					cmd_key,
					tk_key,
					cmd_token.type,
					sql_token.token_name, 
					sql_token.text)
				break
			end

			if cmd_token.func then
				local check_res, check_errmsg = cmd_token.func(sql_token)

				if not check_res then
					is_error = true
					error_msg = check_errmsg or ("parse error at '%s' in '%s'"):format(
						sql_token.text,
						cmd.query)
					break
				end
			end
		end

		if is_error and error_is_fatal then
			-- we got a fatal error, don't continue with parsing
			break
		end

		if not is_error then
			-- command was parsed successful
			break
		end
	end

	if is_error and error_is_fatal then
		proxy.response = {
			type = proxy.MYSQLD_PACKET_ERR,
			errmsg = error_msg or "(auto-config) parsing query failed, but not error-msg was set"
		}
	elseif is_error then
		-- looks like we matched no statement
		return nil 
	else
		proxy.response.type = proxy.MYSQLD_PACKET_OK
	end

	return proxy.PROXY_SEND_RESULT
end

---
-- transform a table into loadable string
local function tbl2str(tbl, indent)
	local s = ""
	indent = indent or "" -- set a default

	for k, v in pairs(tbl) do
		s = s .. indent .. ("[%q] = "):format(k)
		if type(v) == "table" then
			s = s .. "{\n" .. tbl2str(v, indent .. "  ") .. indent .. "}"
		elseif type(v) == "string" then
			s = s .. ("%q"):format(v)
		else
			s = s .. tostring(v)
		end
		s = s .. ",\n"
	end

	return s
end

function save(tbl, filename)
	local content = "return {\n" .. tbl2str(tbl, "  ") .. "}"

	local file, errmsg = io.open(filename, "w")

	if not file then
		return false, errmsg
	end

	file:write(content)

	return true
end

function load(tbl, filename)
	local func, errmsg = loadfile(filename)

	if not func then
		return false, errmsg
	end

	local v = func()

	for mod, options in pairs(v) do
		if tbl[mod] then
			-- weave the loaded options in
			for option, value in pairs(options) do
				tbl[mod][option] = value
			end
		else
			tbl[mod] = options
		end
	end

	return true
end

local mt = getmetatable(proxy.global.config) or {}
mt.__index = { 
	handle = handle,
	load = load,
	save = save
}
setmetatable(proxy.global.config, mt)

