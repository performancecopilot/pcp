#!/bin/sh -eux

cd ./artifacts
sudo rpm -iv *.rpm

echo "pcpqa ALL=(ALL) NOPASSWD: ALL" | sudo tee /etc/sudoers.d/pcpqa
echo "127.0.0.1 $(hostname)" | sudo tee -a /etc/hosts
