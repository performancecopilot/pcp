#!/bin/bash

call_endpoint() {
    nc -w 0 -u 0.0.0.0 8125
}

echo "login:1|c"    | call_endpoint
echo "login:3|c"    | call_endpoint
echo "login:5|c"    | call_endpoint
echo "logout:4|c"   | call_endpoint
echo "logout:2|c"   | call_endpoint
echo "logout:2|c"   | call_endpoint

echo "tagged_counter_a,tagX=X:1|c"              | call_endpoint
echo "tagged_counter_a,tagY=Y:2|c"              | call_endpoint
echo "tagged_counter_a,tagZ=Z:3|c"              | call_endpoint
echo "tagged_counter_b:4|c"                     | call_endpoint
echo "tagged_counter_b,tagX=X,tagW=W:5|c"       | call_endpoint

