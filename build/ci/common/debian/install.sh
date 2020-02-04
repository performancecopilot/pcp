#!/bin/sh -eux

cd ./artifacts
sudo dpkg -i *.deb
echo 'pcpqa ALL=(ALL)   NOPASSWD: ALL' | sudo tee /etc/sudoers.d/pcpqa
