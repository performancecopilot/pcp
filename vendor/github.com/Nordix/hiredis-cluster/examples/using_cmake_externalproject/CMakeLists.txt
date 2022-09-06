cmake_minimum_required(VERSION 3.11)
project(hiredis_cluster_externalproject)
include(ExternalProject)

ExternalProject_Add(hiredis
  PREFIX hiredis
  GIT_REPOSITORY  https://github.com/redis/hiredis
  GIT_TAG         v1.0.2
  CMAKE_ARGS
    "-DCMAKE_C_FLAGS:STRING=-std=c99"
    "-DENABLE_SSL:BOOL=ON"
    "-DDISABLE_TESTS:BOOL=ON"
    "-DCMAKE_BUILD_TYPE:STRING=Debug"
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/install"
)

ExternalProject_Add(hiredis_cluster
  PREFIX hiredis_cluster
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.."
  CMAKE_ARGS
    "-DENABLE_SSL:BOOL=ON"
    "-DDISABLE_TESTS:BOOL=ON"
    "-DDOWNLOAD_HIREDIS:BOOL=OFF"
    "-DCMAKE_BUILD_TYPE:STRING=Debug"
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/install"
    "-DCMAKE_PREFIX_PATH:PATH=${CMAKE_CURRENT_BINARY_DIR}/install/usr/local"
  DEPENDS hiredis
)

ExternalProject_Add(examples
  PREFIX examples
  SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src"
  INSTALL_COMMAND ""
  CMAKE_ARGS
    "-DENABLE_SSL:BOOL=ON"
    "-DCMAKE_BUILD_TYPE:STRING=Debug"
    "-DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR}/install"
    "-DCMAKE_PREFIX_PATH:PATH=${CMAKE_CURRENT_BINARY_DIR}/install/usr/local"
  DEPENDS hiredis_cluster
)
