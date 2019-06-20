#!/bin/bash

call_endpoint() {
    nc -w 1 -u 0.0.0.0 8125
}

############################
# Incorrect case - missing count

echo "session_duration:|ms" | call_endpoint
echo "cache_loopup:|ms"     | call_endpoint

## Results:
## Should be thrown away
############################


############################
# Incorrect case - value includes incorrect character

echo "session_duration:1wq|ms"  | call_endpoint
echo "cache_cleared:4ěš|ms"     | call_endpoint
echo "session_started:1_4w|ms"  | call_endpoint

## Results:
## Should be thrown away
############################


############################
# Incorrect case - value is negative

echo "session_started:-1|ms" | call_endpoint
echo "cache_cleared:-4|ms"   | call_endpoint
echo "cache_cleared:-1|ms"   | call_endpoint

## Results:
## This will successfuly get parsed but will be thrown away at later time when trying to update / create metric value in aggregator
############################


############################
# Incorrect case - incorrect type specifier

echo "session_started:1|mss"     | call_endpoint
echo "cache_cleared:4|msd"       | call_endpoint
echo "cache_cleared:1|msa"       | call_endpoint

## Results:
## Should be thrown away
############################


############################
# Incorrect case - value is missing

echo "session_started:|ms"      | call_endpoint

## Results:
## Should be thrown away
############################


############################
# Incorrect case - metric is missing

echo ":20|ms"     | call_endpoint

## Results:
## Should be thrown away
############################
