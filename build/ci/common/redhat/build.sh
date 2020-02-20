#!/bin/sh -eux

yum list installed

[ -d ./pcp ] || git clone https://github.com/performancecopilot/pcp.git
cd ./pcp
./Makepkgs --verbose

cd ..
mkdir -p ./artifacts
mv ./pcp/pcp-*/build/rpm/*.rpm ./artifacts
