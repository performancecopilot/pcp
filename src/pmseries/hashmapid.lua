local id = redis.pcall('HGET', KEYS[1], ARGV[1])
if (id == false) then
    id = redis.pcall('HLEN', KEYS[1]) + 1
    redis.call('HSETNX', KEYS[1], ARGV[1], tostring(id))
end
return id
