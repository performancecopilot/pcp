#!/bin/sh -eu

cd /home/pcp/artifacts
sudo dpkg -i *.deb
echo 'pcpqa ALL=(ALL)   NOPASSWD: ALL' | sudo tee /etc/sudoers.d/pcpqa
