#!/bin/sh -eux

yum update
yum install -y git

git clone "${GIT_REPO}"
cd ./pcp
git checkout "${GIT_COMMIT}"

yum -y --skip-broken install `./qa/admin/check-vm -fp`

cd .. && rm -rf ./pcp
