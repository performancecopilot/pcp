#!/bin/sh

cd /vagrant

cat <<EOF >/etc/systemd/resolved.conf
#  This file is part of systemd.
# 
#  systemd is free software; you can redistribute it and/or modify it
#  under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation; either version 2.1 of the License, or
#  (at your option) any later version.
#
# Entries in this file show the compile time defaults.
# You can change settings by editing this file.
# Defaults can be restored by simply deleting this file.
#
# See resolved.conf(5) for details

[Resolve]
#DNS=4.2.2.1 4.2.2.2 208.67.220.220 208.67.222.222
#DNS=8.8.8.8
#FallbackDNS=
#Domains=
#LLMNR=no
#MulticastDNS=no
#DNSSEC=yes
#Cache=yes
#DNSStubListener=yes
EOF
sudo systemctl restart systemd-resolved
apt-get update
apt-get install -y `./qa/admin/check-vm -p`

sudo -H -u vagrant ./Makepkgs --verbose
dpkg -i build/deb/*.deb
echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/pcpqa
sudo service pmcd restart
sudo service pmlogger restart
