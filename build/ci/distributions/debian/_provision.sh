#!/bin/sh -xe

cd /home/packer
sudo apt-get update
#sudo apt-get dist-upgrade
git clone --branch "$PCP_COMMIT" --depth 1 https://github.com/performancecopilot/pcp.git
for i in `./pcp/qa/admin/check-vm -p`
do
    sudo DEBIAN_FRONTEND=noninteractive apt-get install -y $i || true
done
sudo apt-get install -y zlib1g-dev

sudo /usr/sbin/waagent -force -deprovision+user
export HISTSIZE=0
sync
