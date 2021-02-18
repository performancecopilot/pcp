#!/bin/bash -eux

podman --version

echo Previous conmon
/usr/libexec/podman/conmon --version

wget -q https://github.com/containers/conmon/releases/download/v2.0.26/conmon.amd64
chmod +x conmon.amd64
sudo cp -f conmon.amd64 /usr/bin/conmon
sudo cp -f conmon.amd64 /usr/libexec/podman/conmon

echo Upgraded conmon
/usr/libexec/podman/conmon --version
