### 0.6.0 - Feb 09, 2021
* Minimum required version of CMake changed to 3.11 (from 3.14)
* Re-added the Makefile for symmetry with hiredis, which also enables
  support for statically-linked libraries.
* Improved examples
* Corrected crashes and leaks in OOM scenarios
* New API for sending commands to specific node
* New API for node iteration, can be used for sending commands
  to some or all nodes.

### 0.5.0 - Dec 07, 2020

* Renamed to [hiredis-cluster](https://github.com/Nordix/hiredis-cluster)
* The C library `hiredis` is an external dependency rather than a builtin part
  of the cluster client, meaning that `hiredis` v1.0.0 or later can be used.
* Support for SSL/TLS introduced in Redis 6
* Support for IPv6
* Support authentication using AUTH
* Handle variable number of keys in command EXISTS
* Improved CMake build
* Code style guide (using clang-format)
* Improved testing
* Memory leak corrections and allocation failure handling

### 0.4.0 - Jan 24, 2019

* Updated underlying hiredis version to 0.14.0
* Added CMake files to enable Windows and Mac builds
* Fixed bug due to CLUSTER NODES reply format change

https://github.com/heronr/hiredis-vip

### 0.3.0 - Dec 07, 2016

* Support redisClustervCommand, redisClustervAppendCommand and redisClustervAsyncCommand api. (deep011)
* Add flags HIRCLUSTER_FLAG_ADD_OPENSLOT and HIRCLUSTER_FLAG_ROUTE_USE_SLOTS. (deep011)
* Support redisClusterCommandArgv related api. (deep011)
* Fix some serious bugs. (deep011)

https://github.com/vipshop/hiredis-vip

### 0.2.1 - Nov 24, 2015

This release support redis cluster api.

* Add hiredis 0.3.1. (deep011)
* Support cluster synchronous API. (deep011)
* Support multi-key command(mget/mset/del) for redis cluster. (deep011)
* Support cluster pipelining. (deep011)
* Support cluster asynchronous API. (deep011)
