#!/bin/bash

call_endpoint() {
    nc -w 0 -u 0.0.0.0 8125
}

############################
# Simple correct case

echo "login2:1|c"    | call_endpoint
echo "login2:3|c"    | call_endpoint
echo "login2:5|c"    | call_endpoint
echo "logout2:4|c"   | call_endpoint
echo "logout2:2|c"   | call_endpoint
echo "logout2:2|c"   | call_endpoint

## Results:
## login = 9
## logout = 8
############################
