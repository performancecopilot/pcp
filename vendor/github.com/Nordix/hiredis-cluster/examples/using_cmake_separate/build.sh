#!/bin/sh
set -e
script_dir=$(realpath "${0%/*}")

# Download hiredis
hiredis_version=1.0.0
curl -L https://github.com/redis/hiredis/archive/v${hiredis_version}.tar.gz | tar -xz -C ${script_dir}

# Build and install hiredis using CMake
mkdir -p ${script_dir}/hiredis_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISABLE_TESTS=ON -DENABLE_SSL=ON \
      -DCMAKE_C_FLAGS="-std=c99" \
      -S ${script_dir}/hiredis-${hiredis_version} -B ${script_dir}/hiredis_build
make -C ${script_dir}/hiredis_build DESTDIR=${script_dir}/install install


# Download hiredis-cluster
hiredis_cluster_version=0.5.0
curl -L https://github.com/Nordix/hiredis-cluster/archive/${hiredis_cluster_version}.tar.gz | tar -xz -C ${script_dir}

# Build and install hiredis-cluster using CMake
# Uses '-Dxxxx_DIR' defines that points to hiredis-config.cmake and hiredis_ssl-config.cmake installed
# above. These files describes the dependecy packages.
mkdir -p ${script_dir}/hiredis_cluster_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DDISABLE_TESTS=ON -DENABLE_SSL=ON -DDOWNLOAD_HIREDIS=OFF \
      -DCMAKE_C_FLAGS="-std=c99 -D_XOPEN_SOURCE=600" \
      -Dhiredis_DIR=${script_dir}/install/usr/local/share/hiredis \
      -Dhiredis_ssl_DIR=${script_dir}/install/usr/local/share/hiredis_ssl \
      -S ${script_dir}/hiredis-cluster-${hiredis_cluster_version} -B ${script_dir}/hiredis_cluster_build
make -C ${script_dir}/hiredis_cluster_build DESTDIR=${script_dir}/install install


# Build examples from this repo. but link with above built libraries
# installed in ${script_dir}/install/
mkdir -p ${script_dir}/example_build
cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_FLAGS="-std=c99" \
      -Dhiredis_DIR=${script_dir}/install/usr/local/share/hiredis \
      -Dhiredis_ssl_DIR=${script_dir}/install/usr/local/share/hiredis_ssl \
      -Dhiredis_cluster_DIR=${script_dir}/install/usr/local/share/hiredis_cluster \
      -S ${script_dir}/../src -B ${script_dir}/example_build
make -C ${script_dir}/example_build
