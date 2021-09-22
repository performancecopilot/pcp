#!/bin/sh
set -e

# This script builds and installs hiredis and hiredis-cluster using
# CMakes ExternalProject module.
# The shared library variants are used when building the examples.

script_dir=$(realpath "${0%/*}")

# Prepare a build directory
mkdir -p ${script_dir}/build
cd ${script_dir}/build

# Generate makefiles
cmake ..

# Build
VERBOSE=1 make
