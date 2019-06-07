#!/bin/bash

call_endpoint() {
    nc -w 1 -u 0.0.0.0 8125
}

############################
# Incorrect case - value includes incorrect character

echo "session_started:1wq|g"    | call_endpoint
echo "cache_cleared:4ěš|g"      | call_endpoint
echo "session_started:1_4w|g"   | call_endpoint

## Results:
## Should be thrown away
############################


############################
# Incorrect case - value is negative NaN

echo "session_started:-we|g"    | call_endpoint
echo "cache_cleared:-0ě2|g"     | call_endpoint
echo "cache_cleared:-02x|g"     | call_endpoint

## Results:
## Should be thrown away
############################


############################
# Incorrect case - incorrect type specifier

echo "session_started:1|gx"     | call_endpoint
echo "cache_cleared:4|gw"       | call_endpoint
echo "cache_cleared:1|rg"       | call_endpoint

## Results:
## Should be thrown away
############################


############################
# Incorrect case - value is missing

echo "session_started:|g"   | call_endpoint
echo "cache_cleared:|g"     | call_endpoint

## Results:
## Should be thrown away
############################
