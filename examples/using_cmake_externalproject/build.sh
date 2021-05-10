#!/bin/sh
set -e
script_dir=$(realpath "${0%/*}")

# Create build directory
mkdir -p ${script_dir}/build

# Generate makefiles
cmake -B ${script_dir}/build -S ${script_dir}

# Build
VERBOSE=1 make -C ${script_dir}/build
