#!/bin/sh -eux

sudo apt-get update
sudo DEBIAN_FRONTEND=noninteractive apt-get -y dist-upgrade
sudo apt-get install -y git rsync

git clone "${GIT_REPO}"
cd pcp
git checkout "${GIT_COMMIT}"

for i in `./qa/admin/check-vm -p`
do
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y $i || true
done
sudo apt-get install -y zlib1g-dev

sudo waagent -force -deprovision+user
export HISTSIZE=0
sync
