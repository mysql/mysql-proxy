if proxy.global.lm_results == nil then
    proxy.global.lm_results = {}
end

function read_query (packet)
    proxy.global.lm_results['read_query2'] = proxy.global.lm_results['read_query2'] or 0
    proxy.global.lm_results['read_query2'] = proxy.global.lm_results['read_query2'] + 1
	if packet:byte() ~= proxy.COM_QUERY then
		return
	end
	local query = packet:sub(2)
    if query:match('select pload status') then
        local header = { 'function', 'hits' } 
        local rows = {}
        for func, hits in pairs(proxy.global.lm_results) do
            table.insert(rows, { func, hits } )
        end
        return proxy.global.make_dataset(header,rows)
    end
end

