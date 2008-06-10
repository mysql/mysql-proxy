
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


