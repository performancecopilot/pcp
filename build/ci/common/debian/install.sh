#!/bin/sh -eu

cd ./artifacts
sudo dpkg -i *.deb
echo 'pcpqa ALL=(ALL)   NOPASSWD: ALL' | sudo tee /etc/sudoers.d/pcpqa

# "invoke-rc.d pmcd start" (inside pcp.postinst.tail hook) doesn't start pmcd on ubuntu 16.04 (works fine on ubuntu 18.04)
# make sure it's started here, as pmcd will create the file /var/lib/pcp/pmns/stdpmid, which is required by QA tests
sudo systemctl start pmcd
