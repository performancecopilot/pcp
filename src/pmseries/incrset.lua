local link_id = redis.call("INCR", KEYS[1])
redis.call("HSET", KEYS[2], link_id, ARGV[1])
return link_id
