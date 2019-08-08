#!/bin/bash

call_endpoint() {
    nc -w 0 -u 0.0.0.0 8125
}

i=0

# printf "example:1|g\nexample:20|g" | call_endpoint
# echo "example:2|c" | call_endpoint
# echo "example:$i|g" | call_endpoint
# echo "example:-$i|g" | call_endpoint
# echo "example.counter:1|c" | call_endpoint
# echo "example.counter:$i|c" | call_endpoint
# echo "example.counter_tens:10|c" | call_endpoint
# echo "example.counter_random:$RANDOM|c" | call_endpoint
# echo "example.timer:$RANDOM|ms" | call_endpoint
# echo "example.gauge:-$i|g" | call_endpoint
echo "example.counter,instance=1:1|c" | call_endpoint
echo "example.gauge,instance=0:+$i|g" | call_endpoint
echo "example.gauge,instance=bazinga:-$i|g" | call_endpoint
echo "example.gauge,x=10:-$i|g" | call_endpoint
echo "example.counter,tagY=Y,tagX=X:1|c" | call_endpoint
echo "example.gauge,x=10,y=20:-$i|g" | call_endpoint
echo "example.counter,tagY=Y,tagX=X,instance=1:1|c" | call_endpoint
echo "example.gauge,i=30,b=10,instance=1:$RANDOM|g" | call_endpoint
echo "example.gauge,x=10,y=20,instance=bazinga:-$i|g" | call_endpoint
echo "example.g a-u/g e,instance=bazinga:-$i|g" | call_endpoint
echo "example.xd,c=20,b=10,a=20,d=10,e=20:100|c" | call_endpoint
echo "example.xd,a=20,b=10,c=20,d=10,e=20:10|c" | call_endpoint
echo "example.xd,b=20,a=10,d=20,c=10,e=20:10|c" | call_endpoint
