#!/bin/sh

cd /vagrant

# setup epel repo
cat <<EOM >/etc/yum.repos.d/epel-bootstrap.repo
[epel]
name=Bootstrap EPEL
mirrorlist=http://mirrors.fedoraproject.org/mirrorlist?repo=epel-\\\$releasever&arch=\\\$basearch
failovermethod=priority
enabled=0
gpgcheck=0
EOM
yum --enablerepo=epel -y install epel-release
rm -f /etc/yum.repos.d/epel-bootstrap.repo

# setup vm
packages=`./qa/admin/check-vm -p`
yum install -y $packages

# build pcp
sudo -H -u vagrant ./Makepkgs

# install pcp
. ./VERSION.pcp
version="$PACKAGE_MAJOR.$PACKAGE_MINOR.$PACKAGE_REVISION"
rpm -Uvh pcp-$version/build/rpm/*.rpm

# setup pcpqa
echo 'pcpqa   ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/pcpqa
