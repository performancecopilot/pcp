#!/bin/sh -eux

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get -y dist-upgrade
apt-get install -y git rsync

git clone "${GIT_REPO}"
cd ./pcp
git checkout "${GIT_COMMIT}"

for i in `./qa/admin/check-vm -p`
do
    apt-get install -y $i || true
done
apt-get install -y zlib1g-dev

cd .. && rm -rf ./pcp
