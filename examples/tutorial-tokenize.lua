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
--[[

   

--]]

local tokenizer = require("proxy.tokenizer")

function read_query(packet) 
	if packet:byte() == proxy.COM_QUERY then
		local tokens = tokenizer.tokenize(packet:sub(2))

		-- just for debug
		for i = 1, #tokens do
			-- print the token and what we know about it
            		local token = tokens[i]
			local txt = token["text"]
            		if token["token_name"] == 'TK_STRING' then
                		txt = string.format("%q", txt)
            		end
			-- print(i .. ": " .. " { " .. token["token_name"] .. ", " .. token["text"] .. " }" )
			print(i .. ": " .. " { " .. token["token_name"] .. ", " .. txt .. " }" )


		end

		print("normalized query: " .. tokenizer.normalize(tokens))
        print("")
	end
end
