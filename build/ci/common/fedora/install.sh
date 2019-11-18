#!/bin/sh -eux

cd /home/pcp/artifacts
sudo rpm -i *.rpm
echo 'pcpqa ALL=(ALL)   NOPASSWD: ALL' | sudo tee /etc/sudoers.d/pcpqa
