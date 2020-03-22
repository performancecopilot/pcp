#!/bin/sh -eux

yum -y update
yum install -y git

yum -y --skip-broken install $(./qa/admin/check-vm -fp)
rm -rf ./qa

if which systemctl; then
    systemctl enable redis
else
    chkconfig redis on
fi
