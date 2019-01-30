#!/bin/sh

apt-get update
cd /vagrant || exit

packages=`./qa/admin/check-vm -p`
sudo apt-get install -y $packages


sudo -H -u vagrant ./Makepkgs
dpkg -i build/deb/*.deb

echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers
