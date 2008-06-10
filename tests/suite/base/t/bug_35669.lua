function faulty (x)
   if x = 0 then                     -- this is wrong!!!
       print('should not come here')
   end
end

function read_query (packet)
   faulty(1)
end
