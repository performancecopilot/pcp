#!/bin/bash

call_endpoint() {
    nc -w 1 -u 0.0.0.0 8125
}

############################
# Simple correct case

echo "login:1|c"    | call_endpoint
echo "login:3|c"    | call_endpoint
echo "login:5|c"    | call_endpoint
echo "logout:4|c"   | call_endpoint
echo "logout:2|c"   | call_endpoint
echo "logout:2|c"   | call_endpoint

## Results:
## login = 9
## logout = 8
############################
