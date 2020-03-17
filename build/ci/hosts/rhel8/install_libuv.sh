#!/bin/sh -eux

dnf install -y https://dl.fedoraproject.org/pub/epel/epel-release-latest-8.noarch.rpm

# upgrade epel-release to 8.8 (the link above contains epel-release 8.7, which does not include modular repositories)
dnf update -y epel-release

# libuv/devel module requires libuv to be installed, see BZ 1809314
dnf install -y libuv

dnf module install -y libuv/devel
