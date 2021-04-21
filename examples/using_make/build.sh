#!/bin/sh
set -e

# This script builds hiredis and hiredis-cluster using Make directly.
# The static library variants are used when building the examples.

script_dir=$(realpath "${0%/*}")
repo_dir=$(realpath "${script_dir}/../../")

# Download hiredis
hiredis_version=1.0.0
curl -L https://github.com/redis/hiredis/archive/v${hiredis_version}.tar.gz | tar -xz -C ${script_dir}

# Build and install hiredis using Make
make -C ${script_dir}/hiredis-${hiredis_version} DESTDIR=${script_dir}/install USE_SSL=1 all install


# Download hiredis-cluster
hiredis_cluster_version=0.5.0
curl -L https://github.com/Nordix/hiredis-cluster/archive/${hiredis_cluster_version}.tar.gz | tar -xz -C ${script_dir}

# Copy makefile since its not available in v0.5.0
cp ${repo_dir}/Makefile ${script_dir}/hiredis-cluster-${hiredis_cluster_version}/

# Build and install hiredis-cluster using Make
make -C ${script_dir}/hiredis-cluster-${hiredis_cluster_version} \
     CFLAGS="-I${script_dir}/install/usr/local/include -D_XOPEN_SOURCE=600" LDFLAGS="-L${script_dir}/install/usr/local/lib" USE_SSL=1 clean all
make -C ${script_dir}/hiredis-cluster-${hiredis_cluster_version} \
     DESTDIR=${script_dir}/install install


# Build example binaries by providing static libraries
make -C ${repo_dir} CFLAGS="-I${script_dir}/install/usr/local/include" \
     LDFLAGS="${script_dir}/install/usr/local/lib/libhiredis_cluster.a ${script_dir}/install/usr/local/lib/libhiredis.a ${script_dir}/install/usr/local/lib/libhiredis_ssl.a" \
     USE_SSL=1 clean examples


# Run simple example:
# ./examples/hiredis-cluster-example
#
# To get a simple Redis Cluster to run towards:
# docker run --name docker-cluster -d -p 7000-7006:7000-7006 "bjosv/redis-cluster:latest"
#

# Run TLS/SSL example:
# ./examples/hiredis-cluster-example-tls
#
# Prepare a Redis Cluster to run towards:
# openssl genrsa -out ca.key 4096
# openssl req -x509 -new -nodes -sha256 -key ca.key -days 3650 -subj '/CN=Redis Test CA' -out ca.crt
# openssl genrsa -out redis.key 2048
# openssl req -new -sha256 -key redis.key -subj '/CN=Redis Server Test Cert' | openssl x509 -req -sha256 -CA ca.crt -CAkey ca.key -CAserial ca.txt -CAcreateserial -days 365 -out redis.crt
# openssl genrsa -out client.key 2048
# openssl req -new -sha256 -key client.key -subj '/CN=Redis Client Test Cert' | openssl x509 -req -sha256 -CA ca.crt -CAkey ca.key -CAserial ca.txt -CAcreateserial -days 365 -out client.crt
#
# chmod 777 redis.key
#
# docker run --name redis-tls-1 -d --net=host -v $PWD:/tls:ro redis:6.0.9 redis-server --cluster-enabled yes --tls-cluster yes --port 0 --tls-ca-cert-file /tls/ca.crt --tls-cert-file /tls/redis.crt --tls-key-file /tls/redis.key --tls-port 7301
# docker run --name redis-tls-2 -d --net=host -v $PWD:/tls:ro redis:6.0.9 redis-server --cluster-enabled yes --tls-cluster yes --port 0 --tls-ca-cert-file /tls/ca.crt --tls-cert-file /tls/redis.crt --tls-key-file /tls/redis.key --tls-port 7302
# docker run --name redis-tls-3 -d --net=host -v $PWD:/tls:ro redis:6.0.9 redis-server --cluster-enabled yes --tls-cluster yes --port 0 --tls-ca-cert-file /tls/ca.crt --tls-cert-file /tls/redis.crt --tls-key-file /tls/redis.key --tls-port 7303
# docker run --name redis-tls-4 -d --net=host -v $PWD:/tls:ro redis:6.0.9 redis-server --cluster-enabled yes --tls-cluster yes --port 0 --tls-ca-cert-file /tls/ca.crt --tls-cert-file /tls/redis.crt --tls-key-file /tls/redis.key --tls-port 7304
# docker run --name redis-tls-5 -d --net=host -v $PWD:/tls:ro redis:6.0.9 redis-server --cluster-enabled yes --tls-cluster yes --port 0 --tls-ca-cert-file /tls/ca.crt --tls-cert-file /tls/redis.crt --tls-key-file /tls/redis.key --tls-port 7305
# docker run --name redis-tls-6 -d --net=host -v $PWD:/tls:ro redis:6.0.9 redis-server --cluster-enabled yes --tls-cluster yes --port 0 --tls-ca-cert-file /tls/ca.crt --tls-cert-file /tls/redis.crt --tls-key-file /tls/redis.key --tls-port 7306
#
# echo 'yes' | docker run --name redis-cli-tls -i --rm --net=host -v $PWD:/tls:ro redis:6.0.9 redis-cli --cluster create --tls --cacert /tls/ca.crt --cert /tls/redis.crt --key /tls/redis.key 127.0.0.1:7301 127.0.0.1:7302 127.0.0.1:7303 127.0.0.1:7304 127.0.0.1:7305 127.0.0.1:7306 --cluster-replicas 1
