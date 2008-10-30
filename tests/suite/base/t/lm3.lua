--[[ $%BEGINLICENSE%$
 Copyright (C) 2007 MySQL AB, 

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

function read_query (packet)
	if packet:byte() ~= proxy.COM_QUERY then
		return
	end
	local query = packet:sub(2)
    if query == 'select 1000' then
        return proxy.global.simple_dataset('result','one thousand')
    end
end

function read_query_result(inj)

end

