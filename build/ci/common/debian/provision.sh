#!/bin/sh -eux

export DEBIAN_FRONTEND=noninteractive
sudo apt-get update
sudo apt-get -y dist-upgrade
sudo apt-get install -y git rsync

git clone "${GIT_REPO}"
cd pcp
git checkout "${GIT_COMMIT}"

for i in `./qa/admin/check-vm -p`
do
    sudo apt-get install -y $i || true
done
sudo apt-get install -y zlib1g-dev

sudo waagent -force -deprovision+user
export HISTSIZE=0
sync
