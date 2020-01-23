#!/bin/sh -eux

cd ./artifacts
sudo rpm -iv *.rpm
echo 'pcpqa ALL=(ALL)   NOPASSWD: ALL' | sudo tee /etc/sudoers.d/pcpqa
