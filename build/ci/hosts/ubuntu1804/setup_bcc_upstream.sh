#!/bin/sh -eux

# python3-bpfcc version 0.5.0 in Ubuntu 18.04 repository doesn't work with linux kernel 5.0.0
# let's install upstream BCC packages

apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 4052245BD4284CDD
echo "deb https://repo.iovisor.org/apt/bionic bionic main" > /etc/apt/sources.list.d/iovisor.list
apt-get update

# by installing python3-bcc first, the check-vm script doesn't attempt to install python3-bpfcc from upstream Ubuntu
apt-get install -y python3-bcc
