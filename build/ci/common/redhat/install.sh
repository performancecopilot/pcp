#!/bin/sh -eux

cd ./artifacts
sudo rpm -iv *.rpm
echo 'pcpqa ALL=(ALL)   NOPASSWD: ALL' | sudo tee /etc/sudoers.d/pcpqa

# start redis if available (required for some QA tests)
sudo systemctl start redis || true
