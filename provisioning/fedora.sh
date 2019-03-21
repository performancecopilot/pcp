#!/bin/sh

cd /vagrant || exit

# setup vm
packages=`./qa/admin/check-vm -fp`
dnf -y -b --skip-broken install $packages

# build pcp
sudo -H -u vagrant ./Makepkgs

# install pcp
. ./VERSION.pcp
version="$PACKAGE_MAJOR.$PACKAGE_MINOR.$PACKAGE_REVISION"
rpm -Uvh --force "pcp-$version/build/rpm/*.rpm"

# setup pcpqa
sed -i '/requiretty/d' /etc/sudoers	# for sudo to work from a script
echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/pcpqa
