#!/bin/bash

set -x -euo pipefail

BPFTRACE_VERSION=${BPFTRACE_VERSION:-0.22.1}
BIN_DIR=/usr/local/bin
sudo mkdir -p $BIN_DIR

if [ $(uname -m) == "x86_64" ]; then
     URL=https://github.com/bpftrace/bpftrace/releases/download/v${BPFTRACE_VERSION}/bpftrace
elif [ $(uname -m) == "aarch64" ]; then
     URL=https://github.com/theihor/bpftrace/releases/download/v${BPFTRACE_VERSION}-arm64/bpftrace-arm64
else
    echo "Unexpected arch: $(uname -m)"
    exit 1
fi

sudo curl -L -o $BIN_DIR/bpftrace $URL
sudo chmod +x $BIN_DIR/bpftrace

# mount tracefs to avoid warnings from bpftrace
grep -q tracefs /proc/mounts || mount -t tracefs tracefs /sys/kernel/tracing

# sanity check
bpftrace --version

