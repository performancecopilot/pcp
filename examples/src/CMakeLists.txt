cmake_minimum_required(VERSION 3.11)
project(examples LANGUAGES C)

# Handle libevent and hiredis
find_library(EVENT_LIBRARY event HINTS /usr/lib/x86_64-linux-gnu)
find_package(hiredis REQUIRED)
find_package(hiredis_cluster REQUIRED)

# Executable: IPv4
add_executable(example_ipv4 example.c)
target_link_libraries(example_ipv4
  hiredis_cluster::hiredis_cluster)

# Executable: async
add_executable(example_async example_async.c)
target_link_libraries(example_async
  hiredis_cluster::hiredis_cluster
  ${EVENT_LIBRARY})

# Executable: tls
if(ENABLE_SSL)
  find_package(hiredis_ssl REQUIRED)
  find_package(hiredis_cluster_ssl REQUIRED)

  add_executable(example_tls example_tls.c)
  target_link_libraries(example_tls
    hiredis_cluster::hiredis_cluster
    hiredis_cluster::hiredis_cluster_ssl
    ${EVENT_LIBRARY})
endif()
