#!/bin/sh -eux

echo "127.0.0.1 $(hostname)" | sudo tee -a /etc/hosts

cd ./artifacts
sudo dpkg -i ./*.deb
echo "pcpqa ALL=(ALL) NOPASSWD: ALL" | sudo tee /etc/sudoers.d/pcpqa
