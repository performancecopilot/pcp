#!/bin/bash

call_endpoint() {
    nc -w 1 -u 0.0.0.0 8125
}

############################
# Incorrect case - value is negative

echo "session_started:-1|c" | call_endpoint
echo "cache_cleared:-4|c"   | call_endpoint
echo "cache_cleared:-1|c"   | call_endpoint

## Results:
## This will successfuly get parsed but will be thrown away at later time when trying to update / create metric value in consumer
############################

############################
# Incorrect case - value includes incorrect character

echo "session_started:1wq|c"    | call_endpoint
echo "cache_cleared:4ěš|c"      | call_endpoint
echo "session_started:1_4w|c"   | call_endpoint

## Results:
## Should be thrown away
############################

############################
# Incorrect case - incorrect type specifier

echo "session_started:1|cx"     | call_endpoint
echo "cache_cleared:4|cw"       | call_endpoint
echo "cache_cleared:1|rc"       | call_endpoint

## Results:
## Should be thrown away
############################

############################
# Incorrect case - value is missing

echo "session_started:|c"     | call_endpoint

## Results:
## Should be thrown away
############################

############################
# Incorrect case - metric is missing

echo ":20|c"     | call_endpoint

## Results:
## Should be thrown away
############################
