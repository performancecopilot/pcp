#!/bin/sh -eux

apt list --installed

[ -d ./pcp ] || git clone https://github.com/performancecopilot/pcp.git
cd ./pcp
./Makepkgs --verbose --check

cd ..
mkdir -p ./artifacts
mv ./pcp/build/deb/*.deb ./artifacts
