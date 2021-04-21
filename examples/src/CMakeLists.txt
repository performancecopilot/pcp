cmake_minimum_required(VERSION 3.14)
project(examples LANGUAGES C)

# Handle libevent and hiredis
find_library(EVENT_LIBRARY event HINTS /usr/lib/x86_64-linux-gnu)
find_package(hiredis REQUIRED)
find_package(hiredis_ssl REQUIRED)
find_package(hiredis_cluster REQUIRED)

add_definitions(-DSSL_SUPPORT)

include_directories("${hiredis_INCLUDE_DIRS}")
include_directories("${hiredis_cluster_INCLUDE_DIRS}")

# Executable: IPv4
add_executable(example_ipv4 example.c)
target_link_libraries(example_ipv4
  ${hiredis_LIBRARIES}
  ${hiredis_cluster_LIBRARIES}
  ${hiredis_ssl_LIBRARIES})

# Executable: async
add_executable(example_async example_async.c)
target_link_libraries(example_async
  ${hiredis_LIBRARIES}
  ${hiredis_cluster_LIBRARIES}
  ${hiredis_ssl_LIBRARIES}
  ${EVENT_LIBRARY})

# Executable: tls
add_executable(example_tls example_tls.c)
target_link_libraries(example_tls
  ${hiredis_LIBRARIES}
  ${hiredis_cluster_LIBRARIES}
  ${hiredis_ssl_LIBRARIES}
  ${EVENT_LIBRARY})
