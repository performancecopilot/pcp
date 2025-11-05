#!/bin/bash

set -euo pipefail

if [ "$COMPILER" == "llvm" ]; then
        export CC="clang-${COMPILER_VERSION}"
        export CXX="clang++-${COMPILER_VERSION}"
        export AR="llvm-ar-${COMPILER_VERSION}"
        export LD="lld-${COMPILER_VERSION}"
elif [ "$COMPILER" == "gcc" ]; then
        export CC="gcc-${COMPILER_VERSION}"
        export CXX="g++-${COMPILER_VERSION}"
        export AR="gcc-ar-${COMPILER_VERSION}"
fi

make -C tests clean
make SHARED=${SHARED:-0} -C tests -j$(nproc) build
