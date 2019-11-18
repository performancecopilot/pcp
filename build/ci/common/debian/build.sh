#!/bin/sh -eux

cd /home/pcp
rm -rf pcp
git clone "${GIT_REPO}"
cd pcp
git checkout "${GIT_COMMIT}"
./Makepkgs --verbose

cd /home/pcp
mkdir -p ./artifacts
mv ./pcp/build/deb/*.deb ./artifacts
