#!/bin/sh

cd /vagrant

apt-get update
apt-get install -y `./qa/admin/check-vm -p`

sudo -H -u vagrant ./Makepkgs
dpkg -i build/deb/*.deb
echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/pcpqa
