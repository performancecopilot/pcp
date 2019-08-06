#!/bin/bash

call_endpoint() {
    nc -w 0 -u 0.0.0.0 8125
}

echo "example,tagY=Y:2|c" | call_endpoint
