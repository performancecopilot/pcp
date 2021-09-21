#!/bin/sh
set -e

# This script builds and installs hiredis and hiredis-cluster as separate
# steps using CMake.
# The shared library variants are used when building the examples.

script_dir=$(realpath "${0%/*}")
repo_dir=$(git rev-parse --show-toplevel)

# Download hiredis
hiredis_version=1.0.0
curl -L https://github.com/redis/hiredis/archive/v${hiredis_version}.tar.gz | tar -xz -C ${script_dir}

# Build and install downloaded hiredis using CMake
mkdir -p ${script_dir}/hiredis_build
cd ${script_dir}/hiredis_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISABLE_TESTS=ON -DENABLE_SSL=ON \
      -DCMAKE_C_FLAGS="-std=c99" \
      ${script_dir}/hiredis-${hiredis_version}
make DESTDIR=${script_dir}/install install


# Build and install hiredis-cluster from the repo using CMake
# Uses '-Dxxxx_DIR' defines that points to hiredis-config.cmake and hiredis_ssl-config.cmake installed
# above. These files defines paths and variables needed by CMake.
mkdir -p ${script_dir}/hiredis_cluster_build
cd ${script_dir}/hiredis_cluster_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISABLE_TESTS=ON -DENABLE_SSL=ON -DDOWNLOAD_HIREDIS=OFF \
      -DCMAKE_C_FLAGS="-std=c99 -D_XOPEN_SOURCE=600" \
      -Dhiredis_DIR=${script_dir}/install/usr/local/share/hiredis \
      -Dhiredis_ssl_DIR=${script_dir}/install/usr/local/share/hiredis_ssl \
      ${repo_dir}
make DESTDIR=${script_dir}/install clean install


# Build examples using headers and libraries installed in previous steps.
mkdir -p ${script_dir}/example_build
cd ${script_dir}/example_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_FLAGS="-std=c99" \
      -Dhiredis_DIR=${script_dir}/install/usr/local/share/hiredis \
      -Dhiredis_ssl_DIR=${script_dir}/install/usr/local/share/hiredis_ssl \
      -Dhiredis_cluster_DIR=${script_dir}/install/usr/local/share/hiredis_cluster \
      ${script_dir}/../src
make
