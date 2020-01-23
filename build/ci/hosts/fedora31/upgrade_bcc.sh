#!/bin/sh -eux

# bcc 0.10.0 doesn't work with kernel 5.4+, therefore enable this repo to upgrade bcc to 0.12.0
sudo dnf upgrade -y --enablerepo=updates-testing --advisory=FEDORA-2020-1e5d1064c1
