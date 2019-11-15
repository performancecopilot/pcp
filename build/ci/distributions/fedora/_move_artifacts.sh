#!/bin/sh -xe

cd /home/pcp
mkdir -p ./artifacts
mv ./pcp/pcp-*/build/rpm/*.rpm ./artifacts
