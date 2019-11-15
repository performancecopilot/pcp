#!/bin/sh -xe

cd /home/packer
#sudo dnf upgrade
sudo dnf install -y git
git clone --branch "$PCP_COMMIT" --depth 1 https://github.com/performancecopilot/pcp.git
sudo dnf -y -b --skip-broken install `./pcp/qa/admin/check-vm -fp`
sudo dnf -y install python2-devel

sudo /usr/sbin/waagent -force -deprovision+user
export HISTSIZE=0
sync
