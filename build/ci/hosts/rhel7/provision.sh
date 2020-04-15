#!/bin/sh -eux

yum -y update

# yum update disables the rhui-optional-rpms repository, which is required for python3-devel
yum-config-manager --enable rhui-rhel-7-server-rhui-optional-rpms

yum install -y git

yum -y --skip-broken install $(./qa/admin/check-vm -fp)
rm -rf ./qa

if which systemctl; then
    systemctl enable redis
else
    chkconfig redis on
fi
