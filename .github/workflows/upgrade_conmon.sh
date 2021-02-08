#!/bin/bash -eu

podman --version

echo Previous conmon
/usr/libexec/podman/conmon --version

wget -q -O conmon.zip https://github.com/containers/conmon/releases/download/v2.0.26/conmon-2.0.26.zip
unzip conmon.zip
chmod +x result/bin/conmon
sudo cp -f result/bin/conmon /usr/bin/conmon
sudo cp -f result/bin/conmon /usr/libexec/podman/conmon

echo Upgraded conmon
/usr/libexec/podman/conmon --version
