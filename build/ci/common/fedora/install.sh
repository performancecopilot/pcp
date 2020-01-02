#!/bin/sh -eu

cd ./artifacts
sudo rpm -i *.rpm
echo 'pcpqa ALL=(ALL)   NOPASSWD: ALL' | sudo tee /etc/sudoers.d/pcpqa
