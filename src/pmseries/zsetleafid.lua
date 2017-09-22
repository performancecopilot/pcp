local ID = redis.pcall('ZSCORE', KEYS[2], ARGV[1])
if (ID == false or ID == 0) then
    ID = redis.pcall('INCR', KEYS[1])
    redis.call('ZADD', KEYS[2], tostring(ID), ARGV[1])
end
return tonumber(ID)
