#!/bin/sh -eux

sudo dnf -y upgrade
sudo dnf install -y git

git clone "${GIT_REPO}"
cd pcp
git checkout "${GIT_COMMIT}"

sudo dnf -y -b --skip-broken install `./qa/admin/check-vm -fp`
sudo dnf -y install python2-devel

sudo waagent -force -deprovision+user
export HISTSIZE=0
sync
