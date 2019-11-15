#!/bin/sh -xe

cd /home/pcp
mkdir -p ./artifacts
mv ./pcp/build/deb/*.deb ./artifacts
