#!/bin/sh -eux

yum -y update
yum install -y git

yum -y --skip-broken install $(./qa/admin/check-vm -fp)
rm -rf ./qa

# if redis is installed (RHEL 7+), start it on boot
if which redis-server; then systemctl enable redis; fi
