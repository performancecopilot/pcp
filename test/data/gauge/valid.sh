#!/bin/bash

call_endpoint() {
    nc -w 0 -u 0.0.0.0 8125
}

echo "login:1|g"        | call_endpoint
echo "login:3|g"        | call_endpoint
echo "login:5|g"        | call_endpoint
echo "logout:4|g"       | call_endpoint
echo "logout:2|g"       | call_endpoint
echo "logout:2|g"       | call_endpoint
echo "login:+0.5|g"     | call_endpoint
echo "login:-0.12|g"    | call_endpoint
echo "logout:0.128|g"   | call_endpoint

echo "success:0|g"      | call_endpoint
echo "success:+5|g"     | call_endpoint
echo "success:-12|g"    | call_endpoint
echo "error:0|g"        | call_endpoint
echo "error:+9|g"       | call_endpoint
echo "error:-0|g"       | call_endpoint

echo "tagged_gauge_a,tagX=X:1|c"            | call_endpoint
echo "tagged_gauge_a,tagY=Y:+2|c"           | call_endpoint
echo "tagged_gauge_a,tagY=Y:-1|c"           | call_endpoint
echo "tagged_gauge_a,tagZ=Z:-3|c"           | call_endpoint
echo "tagged_gauge_b:4|c"                   | call_endpoint
echo "tagged_gauge_b,tagX=X,tagW=W:-5|c"    | call_endpoint
