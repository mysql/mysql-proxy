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

if proxy.global.lm_results == nil then
    proxy.global.lm_results = {}
end

function read_auth( auth )
    proxy.global.lm_results['read_auth1'] = proxy.global.lm_results['read_auth1'] or 0
    proxy.global.lm_results['read_auth1'] = proxy.global.lm_results['read_auth1'] + 1
end

function connect_server()
    proxy.global.lm_results['connect_server1'] = proxy.global.lm_results['connect_server1'] or 0
    proxy.global.lm_results['connect_server1'] = proxy.global.lm_results['connect_server1'] + 1
end

function read_handshake( auth )
    proxy.global.lm_results['read_handshake1'] = proxy.global.lm_results['read_handshake1'] or 0
    proxy.global.lm_results['read_handshake1'] = proxy.global.lm_results['read_handshake1'] + 1
end

function read_auth_result( auth )
    proxy.global.lm_results['read_auth_result1'] = proxy.global.lm_results['read_auth_result1'] or 0
    proxy.global.lm_results['read_auth_result1'] = proxy.global.lm_results['read_auth_result1'] + 1
end

function disconnect_client()
    proxy.global.lm_results['disconnect_client1'] = proxy.global.lm_results['disconnect_client1'] or 0
    proxy.global.lm_results['disconnect_client1'] = proxy.global.lm_results['disconnect_client1'] + 1
end


