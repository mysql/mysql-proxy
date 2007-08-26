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

module("proxy.parser", package.seeall)

function tablename_expand(tblname)
	if proxy.connection.default_db then
		tblname = proxy.connection.default_db .. "." .. tblname
	end

	return tblname
end

---
-- extract the table-names from tokenized SQL token stream
--
-- @see proxy.tokenize
function get_tables(tokens)
	local sql_stmt = nil
	local in_tablelist = false
	local next_is_tblname = false
	local in_braces = 0

	local tables = {}

	for i, token in ipairs(tokens) do
		-- print(i .. " token: " .. token["token_name"])
		if token["token_name"] == "TK_COMMENT" or
		   token["token_name"] == "TK_UNKNOWN" then

		elseif not sql_stmt then
			-- try to get the SQL stmt
			sql_stmt = token.text:upper()
			-- print(i .. " sql-stmt: " .. sql_stmt)
			
			if sql_stmt == "UPDATE" or
			   sql_stmt == "INSERT" then
				in_tablelist = true
			end
		elseif sql_stmt == "SELECT" or 
		       sql_stmt == "DELETE" then
			-- we have sql_stmt token already, try to get the table names
			--
			-- SELECT ... FROM tbl AS alias, ... 
			-- DELETE FROM ... 
		
			if in_tablelist then
				if token["token_name"] == "TK_COMMA" then
					next_is_tblname = true
				elseif in_braces > 0 then
					-- ignore sub-queries
				elseif token["token_name"] == "TK_SQL_AS" then
					next_is_tblname = false
				elseif token["token_name"] == "TK_SQL_JOIN" then
					next_is_tblname = true
				elseif token["token_name"] == "TK_SQL_LEFT" or
				       token["token_name"] == "TK_SQL_RIGHT" or
				       token["token_name"] == "TK_SQL_OUTER" or
				       token["token_name"] == "TK_SQL_USING" or
				       token["token_name"] == "TK_SQL_ON" or
				       token["token_name"] == "TK_SQL_AND" or
				       token["token_name"] == "TK_SQL_OR" then
					-- ignore me
				elseif token["token_name"] == "TK_LITERAL" and next_is_tblname then
					tables[tablename_expand(token.text)] = (sql_stmt == "SELECT" and "read" or "write")

					next_is_tblname = false
				elseif token["token_name"] == "TK_OBRACE" then
					in_braces = in_braces + 1
				elseif token["token_name"] == "TK_CBRACE" then
					in_braces = in_braces - 1
				elseif token["token_name"] == "TK_SQL_WHERE" or
				       token["token_name"] == "TK_SQL_GROUP" or
				       token["token_name"] == "TK_SQL_ORDER" or
				       token["token_name"] == "TK_SQL_LIMIT" or
				       token["token_name"] == "TK_CBRACE" then
					in_tablelist = false
				else
					print("unknown, found token: " .. token["token_name"] .. " -> " .. token.text)
					-- in_tablelist = false
				end
			elseif token["token_name"] == "TK_SQL_FROM" then
				in_tablelist = true
				next_is_tblname = true
			end
			
			-- print(i .. " in-from: " .. (in_from and "true" or "false"))
			-- print(i .. " next-is-tblname: " .. (next_is_tblname and "true" or "false"))
		elseif sql_stmt == "CREATE" or 
		       sql_stmt == "DROP" or 
		       sql_stmt == "ALTER" or 
		       sql_stmt == "RENAME" then
			-- CREATE TABLE <tblname>
			if not ddl_type then
				ddl_type = token.text:upper()
				in_tablelist = true
			elseif ddl_type == "TABLE" then
				if in_tablelist and 
				   token["token_name"] == "TK_LITERAL" then
					tables[tablename_expand(token.text)] = (sql_stmt == "SELECT" and "read" or "write")
				else
					in_tablelist = false

					break
				end
			end
		elseif sql_stmt == "INSERT" then
			-- INSERT INTO ...
			if in_tablelist then
				if token["token_name"] == "TK_LITERAL" then
					tables[tablename_expand(token.text)] = (sql_stmt == "SELECT" and "read" or "write")
				elseif token["token_name"] == "TK_SQL_INTO" then
				else
					in_tablelist = false
				end
			end
		elseif sql_stmt == "UPDATE" then
			-- UPDATE <tbl> SET ..
			if in_tablelist then
				if token["token_name"] == "TK_LITERAL" then
					tables[tablename_expand(token.text)] = (sql_stmt == "SELECT" and "read" or "write")
				elseif token["token_name"] == "TK_SQL_SET" then
					in_tablelist = false

					break
				end
			end
		end
	end

	return tables
end


