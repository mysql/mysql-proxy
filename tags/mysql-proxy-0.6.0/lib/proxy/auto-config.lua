module("proxy.auto-config", package.seeall)

local tokenizer = require("proxy.tokenizer")

local function parse_value(fld, token)
	local t = type(fld)

	if t == "boolean" then
		if token.token_name == "TK_INTEGER" then
			return (token.text ~= "0")
		end
	end
	
	return
end

function handle(cmd)
	-- handle script-options first
	if cmd.type ~= proxy.COM_QUERY then return nil end
	if cmd.query:sub(1, 3):upper() ~= "SET" then return nil end
	
	local tokens     = assert(tokenizer.tokenize(cmd.query))
	local norm_query = tokenizer.normalize(tokens)

	-- looks like a SET query
	if tokens[1].token_name ~= "TK_SQL_SET" then return end
	if tokens[2].token_name ~= "TK_LITERAL" or tokens[2].text:upper() ~= "GLOBAL" then return end

	local is_ok = false

	if tokens[3].token_name == "TK_LITERAL" and proxy.global.config[tokens[3].text] ~= nil then
		if tokens[4].token_name == "TK_DOT" then
			-- SET GLOBAL <scope>.<key> = <value> 
			if tokens[5].token_name == "TK_LITERAL" and proxy.global.config[tokens[3].text][tokens[5].text] ~= nil then
				if tokens[6].token_name == "TK_EQ" then
					-- next one is the value
					local r = parse_value(proxy.global.config[tokens[3].text][tokens[5].text], tokens[7])

					if r ~= nil then
						proxy.global.config[tokens[3].text][tokens[5].text] = r

						is_ok = true
					end
				end
			end
		elseif tokens[4].token_name == "TK_EQ" then
			-- SET GLOBAL <key> = <value>
			local r = parse_value(proxy.global.config[tokens[3].text], tokens[5])

			if r ~= nil then
				proxy.global.config[tokens[3].text] = r

				is_ok = true
			end
		end
	end

	if not is_ok then return end

	proxy.response = {
		type = proxy.MYSQLD_PACKET_OK
	}

	return proxy.PROXY_SEND_RESULT
end
