#!/bin/sh -eux

# libuv/devel module requires libuv to be installed, see BZ 1809314
dnf install -y libuv

dnf module install -y libuv/devel
