#!/bin/sh -eux

dnf -y upgrade
dnf install -y git

dnf -y -b --skip-broken install $(./qa/admin/check-vm -fp)
rm -rf ./qa

dnf -y install python2-devel
systemctl enable redis
