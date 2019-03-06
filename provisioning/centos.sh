#!/bin/sh

cd /vagrant || exit

# setup epel repo
yum -y clean all && yum -y update
yum -y install epel-release

# setup vm
packages=`./qa/admin/check-vm -fp`
yum install -y $packages

# build pcp
sudo -H -u vagrant ./Makepkgs

# install pcp
. ./VERSION.pcp
version="$PACKAGE_MAJOR.$PACKAGE_MINOR.$PACKAGE_REVISION"
rpm -Uvh --force pcp-$version/build/rpm/*.rpm

# setup pcpqa
echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/pcpqa
