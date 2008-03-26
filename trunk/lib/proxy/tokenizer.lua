--[[

   Copyright (C) 2007, 2008 MySQL AB

--]]

module("proxy.tokenizer", package.seeall)

---
-- normalize a query
--
-- * remove comments
-- * quote literals
-- * turn constants into ?
-- * turn tokens into uppercase
--
-- @param tokens a array of tokens
-- @return normalized SQL query
-- 
-- @see tokenize
function normalize(tokens)
	-- we use a string-stack here and join them at the end
	-- see http://www.lua.org/pil/11.6.html for more
	--
	local stack = {}

	for i, token in ipairs(tokens) do
		-- normalize the query
		if token["token_name"] == "TK_COMMENT" then
		elseif token["token_name"] == "TK_LITERAL" then
			table.insert(stack, "`" .. token.text .. "` ")
		elseif token["token_name"] == "TK_STRING" or
		       token["token_name"] == "TK_INTEGER" or
		       token["token_name"] == "TK_FLOAT" then
			table.insert(stack, "? ")
		elseif token["token_name"] == "TK_FUNCTION" then
			table.insert(stack,  token.text:upper())
		else
			table.insert(stack,  token.text:upper() .. " ")
		end
	end

	return table.concat(stack)
end

---
-- call the included tokenizer
--
-- this function is only a wrapper and exists mostly
-- for constancy and documentation reasons
function tokenize(packet)
	return proxy.tokenize(packet)
end

---
-- return the first command token
--
-- * strips the leading comments
function first_stmt_token(tokens)
	for i, token in ipairs(tokens) do
		-- normalize the query
		if token["token_name"] == "TK_COMMENT" then
		elseif token["token_name"] == "TK_LITERAL" then
			-- commit and rollback at LITERALS
			return token
		else
			-- TK_SQL_* are normal tokens
			return token
		end
	end

	return nil
end

---
--[[

   returns an array of simple token values
   without id and name, and stripping all comments
   
   @param tokens an array of tokens, as produced by the tokenize() function
   @param quote_strings : if set, the string tokens will be quoted
   @see tokenize
--]]
function bare_tokens (tokens, quote_strings)
    local simple_tokens = {}
	for i, token in ipairs(tokens) do
        if (token['token_name'] == 'TK_STRING') and quote_strings then
            table.insert(simple_tokens, string.format('%q', token['text'] ))
        elseif (token['token_name'] ~= 'TK_COMMENT') then
            table.insert(simple_tokens, token['text'])
        end
    end
    return simple_tokens
end

---
--[[
    
   Returns a text query from an array of tokens, stripping off comments
  
   @param tokens an array of tokens, as produced by the tokenize() function
   @param start_item ignores tokens before this one
   @param end_item ignores token after this one
   @see tokenize
--]]
function tokens_to_query ( tokens , start_item, end_item )
    if not start_item then
        start_item = 1
    end
    if not end_item then
        end_item = #tokens
    end
    local counter  = 0
    local new_query = ''
	for i, token in ipairs(tokens) do
        counter = counter + 1
        if (counter >= start_item and counter <= end_item ) then
            if (token['token_name'] == 'TK_STRING') then
                new_query = new_query .. string.format('%q', token['text'] )
            elseif token['token_name'] ~= 'TK_COMMENT' then
                new_query = new_query .. token['text'] 
            end
            if (token['token_name'] ~= 'TK_FUNCTION')
               and 
               (token['token_name'] ~= 'TK_COMMENT') 
            then
                new_query = new_query .. ' '
            end
        end
    end
    return new_query
end

---
--[[
   returns an array of tokens, stripping off all comments

   @param tokens an array of tokens, as produced by the tokenize() function
   @see tokenize, simple_tokens
--]]
function tokens_without_comments (tokens)
    local new_tokens = {}
	for i, token in ipairs(tokens) do
        if (token['token_name'] ~= 'TK_COMMENT') then
            table.insert(new_tokens, token['text'])
        end
    end
    return new_tokens
end

