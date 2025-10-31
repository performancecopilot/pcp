#!/bin/bash

set -euo pipefail

# Assume sudo in this script
GITHUB_WORKSPACE=${GITHUB_WORKSPACE:-$(pwd)}
BPFTRACE_VERSION=${BPFTRACE_VERSION:-0.22.1}
COMPILER=${COMPILER:-gcc}
COMPILER_VERSION=${COMPILER_VERSION:-13}

# Install pre-requisites
apt-get update -y
DEBIAN_FRONTEND=noninteractive apt-get install -y tzdata
apt-get install -y curl file gawk gnupg libfuse2t64 lsb-release make software-properties-common sudo wget

# Install CC
if [ "$COMPILER" == "llvm" ]; then
        curl -O https://apt.llvm.org/llvm.sh
        chmod +x llvm.sh
        ./llvm.sh ${COMPILER_VERSION}
else
        apt-get install -y gcc-${COMPILER_VERSION} g++-${COMPILER_VERSION}
fi

${GITHUB_WORKSPACE}/.github/scripts/install-bpftrace.sh

