#!/bin/sh -eux

: "${GIT_REPO:="https://github.com/performancecopilot/pcp.git"}"
: "${GIT_COMMIT:=master}"

yum list installed

git clone "${GIT_REPO}"
cd ./pcp
git checkout "${GIT_COMMIT}"
./Makepkgs --verbose

cd ..
mkdir -p ./artifacts
mv ./pcp/pcp-*/build/rpm/*.rpm ./artifacts
