
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

