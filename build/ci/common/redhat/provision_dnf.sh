#!/bin/sh -eux

dnf -y upgrade
dnf install -y git

git clone "${GIT_REPO}"
cd ./pcp
git checkout "${GIT_COMMIT}"
dnf -y -b --skip-broken install `./qa/admin/check-vm -fp`
cd .. && rm -rf ./pcp

dnf -y install python2-devel
systemctl enable redis
