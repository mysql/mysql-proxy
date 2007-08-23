function read_query(packet) 
	if packet:byte() == proxy.COM_QUERY then
		local tokens = proxy.tokenize(packet:sub(2))

		for i, token in ipairs(tokens) do
			print(i .. ": " .. " { " .. token["token_id"] .. ", " .. token["text"] .. " }" )
		end
	end
end
