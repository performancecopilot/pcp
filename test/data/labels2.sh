#!/bin/bash

echo "example,tagX=X:3|c" | nc -w 0 -u 0.0.0.0 8125
echo "example:2|c" | nc -w 0 -u 0.0.0.0 8125
