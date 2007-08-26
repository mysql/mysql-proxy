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
	local n_q = ""

	for i, token in ipairs(tokens) do
		-- normalize the query
		if token["token_name"] == "TK_COMMENT" then
		elseif token["token_name"] == "TK_LITERAL" then
			n_q = n_q .. "`" .. token.text .. "` "
		elseif token["token_name"] == "TK_STRING" then
			n_q = n_q .. "? "
		elseif token["token_name"] == "TK_INTEGER" then
			n_q = n_q .. "? "
		elseif token["token_name"] == "TK_FLOAT" then
			n_q = n_q .. "? "
		else
			n_q = n_q .. token.text:upper() .. " "
		end
	end

	return n_q
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
