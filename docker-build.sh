#!/bin/sh

set -e

docker --version

BUILD_NO=${BUILD_NUMBER:-1}

rm -rf BUILD_OUTPUT/*


docker build -t pcp-build  -f Dockerfile-ol7  .
docker run -v "$(pwd)":/src/pcp-build pcp-build

IMAGE_ID=`docker create pcp-build`
docker rm $IMAGE_ID
