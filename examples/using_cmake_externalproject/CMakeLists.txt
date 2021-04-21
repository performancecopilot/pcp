cmake_minimum_required(VERSION 3.14)
project(hiredis_cluster_externalproject)
include(ExternalProject)

ExternalProject_Add(hiredis
  PREFIX hiredis
  GIT_REPOSITORY  https://github.com/redis/hiredis
  GIT_TAG         v1.0.0
  CMAKE_CACHE_ARGS
    "-DENABLE_SSL:BOOL=ON"
    "-DCMAKE_BUILD_TYPE:STRING=Debug"
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/hiredis"
)

ExternalProject_Add(hiredis_cluster
  PREFIX hiredis_cluster
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.."
  CMAKE_CACHE_ARGS
    "-DENABLE_SSL:BOOL=ON"
    "-DDISABLE_TESTS:BOOL=ON"
    "-DDOWNLOAD_HIREDIS:BOOL=OFF"
    "-DCMAKE_BUILD_TYPE:STRING=Debug"
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/hiredis_cluster"
    "-Dhiredis_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/hiredis/share/hiredis"
    "-Dhiredis_ssl_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/hiredis/share/hiredis_ssl"
  DEPENDS hiredis
)

ExternalProject_Add(examples
  PREFIX examples
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src"
  INSTALL_COMMAND ""
  CMAKE_CACHE_ARGS
    "-DCMAKE_BUILD_TYPE:STRING=Debug"
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/examples"
    "-Dhiredis_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/hiredis/share/hiredis"
    "-Dhiredis_ssl_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/hiredis/share/hiredis_ssl"
    "-Dhiredis_cluster_DIR:PATH=${CMAKE_CURRENT_BINARY_DIR}/hiredis_cluster/share/hiredis_cluster"
  DEPENDS hiredis_cluster
)
