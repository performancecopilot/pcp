if(ENABLE_SSL)
  # Generate SSL certs and keys when needed
  set(SSL_CONFIGS ca.crt ca.key ca.txt redis.crt redis.key client.crt client.key)
  add_custom_command(
    OUTPUT ${SSL_CONFIGS}
    COMMAND openssl genrsa -out ca.key 4096
    COMMAND openssl req -x509 -new -nodes -sha256 -key ca.key -days 3650 -subj '/CN=Redis Test CA' -out ca.crt
    COMMAND openssl genrsa -out redis.key 2048
    COMMAND openssl req -new -sha256 -key redis.key -subj '/CN=Redis Server Test Cert' | openssl x509 -req -sha256 -CA ca.crt -CAkey ca.key -CAserial ca.txt -CAcreateserial -days 365 -out redis.crt
    COMMAND openssl genrsa -out client.key 2048
    COMMAND openssl req -new -sha256 -key client.key -subj '/CN=Redis Client Test Cert' | openssl x509 -req -sha256 -CA ca.crt -CAkey ca.key -CAserial ca.txt -CAcreateserial -days 365 -out client.crt
    )
  add_custom_target(generate_tls_configs DEPENDS ${SSL_CONFIGS})

  set(SSL_LIBRARY hiredis_ssl)
endif()

# Targets to setup Redis Clusters for testing
if(ENABLE_IPV6_TESTS)
  set(NO_IPV6 "")
else()
  set(NO_IPV6 "true") # Ignore command
endif()

find_program(Docker_EXECUTABLE docker)
set(CONTAINER "bjosv/redis-cluster:6.2.0") # When supporting password change to: grokzen/redis-cluster:X.X.X

add_custom_target(start
  COMMAND ${Docker_EXECUTABLE} run --name docker-cluster -d -p 7000-7006:7000-7006 ${CONTAINER} || (exit 0)
  COMMAND ${Docker_EXECUTABLE} run --name docker-cluster-passw -d -e INITIAL_PORT=7100 -e PASSWORD="secretword" -p 7100-7106:7100-7106 ${CONTAINER} || (exit 0)
  COMMAND ${NO_IPV6} ${Docker_EXECUTABLE} run --name docker-cluster-ipv6 -d -e INITIAL_PORT=7200 -e IP=:: -e BIND_ADDRESS=::1 --network host ${CONTAINER} || (exit 0)
)
add_custom_target(stop
  COMMAND ${Docker_EXECUTABLE} rm -f  docker-cluster || (exit 0)
  COMMAND ${Docker_EXECUTABLE} rm -f  docker-cluster-passw || (exit 0)
  COMMAND ${NO_IPV6} ${Docker_EXECUTABLE} rm -f  docker-cluster-ipv6 || (exit 0)
)

# Find dependencies
find_library(EVENT_LIBRARY event HINTS /usr/lib/x86_64-linux-gnu)

if(MSVC)
  # MS Visual: Suppress warnings
  add_compile_options("/wd 4267" "/wd 4244")
else()
  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

# Debug mode for tests
set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
# Make sure ctest gives the output when tests fail
list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")

add_executable(ct_async ct_async.c)
target_link_libraries(ct_async hiredis_cluster hiredis ${SSL_LIBRARY} ${EVENT_LIBRARY})
add_test(NAME ct_async COMMAND "$<TARGET_FILE:ct_async>")
set_tests_properties(ct_async PROPERTIES LABELS "CT")

add_executable(ct_commands ct_commands.c)
target_link_libraries(ct_commands hiredis_cluster hiredis ${SSL_LIBRARY})
add_test(NAME ct_commands COMMAND "$<TARGET_FILE:ct_commands>")
set_tests_properties(ct_commands PROPERTIES LABELS "CT")

add_executable(ct_connection ct_connection.c)
target_link_libraries(ct_connection hiredis_cluster hiredis ${SSL_LIBRARY} ${EVENT_LIBRARY})
add_test(NAME ct_connection COMMAND "$<TARGET_FILE:ct_connection>")
set_tests_properties(ct_connection PROPERTIES LABELS "CT")

add_executable(ct_pipeline ct_pipeline.c)
target_link_libraries(ct_pipeline hiredis_cluster hiredis ${SSL_LIBRARY} ${EVENT_LIBRARY})
add_test(NAME ct_pipeline COMMAND "$<TARGET_FILE:ct_pipeline>")
set_tests_properties(ct_pipeline PROPERTIES LABELS "CT")

add_executable(ct_connection_ipv6 ct_connection_ipv6.c)
target_link_libraries(ct_connection_ipv6 hiredis_cluster hiredis ${SSL_LIBRARY} ${EVENT_LIBRARY})
if(ENABLE_IPV6_TESTS)
  add_test(NAME ct_connection_ipv6 COMMAND "$<TARGET_FILE:ct_connection_ipv6>")
  set_tests_properties(ct_connection_ipv6 PROPERTIES LABELS "CT")
endif()

add_executable(ct_out_of_memory_handling ct_out_of_memory_handling.c)
target_link_libraries(ct_out_of_memory_handling hiredis_cluster hiredis ${SSL_LIBRARY} ${EVENT_LIBRARY})
add_test(NAME ct_out_of_memory_handling COMMAND "$<TARGET_FILE:ct_out_of_memory_handling>")
set_tests_properties(ct_out_of_memory_handling PROPERTIES LABELS "CT")

add_executable(ct_specific_nodes ct_specific_nodes.c)
target_link_libraries(ct_specific_nodes hiredis_cluster hiredis ${SSL_LIBRARY} ${EVENT_LIBRARY})
add_test(NAME ct_specific_nodes COMMAND "$<TARGET_FILE:ct_specific_nodes>")
set_tests_properties(ct_specific_nodes PROPERTIES LABELS "CT")

if(ENABLE_SSL)
  # Executable: tls
  add_executable(example_tls main_tls.c)
  target_link_libraries(example_tls hiredis_cluster hiredis ${SSL_LIBRARY})
  add_dependencies(example_tls generate_tls_configs)

  # Executable: async tls
  add_executable(example_async_tls main_async_tls.c)
  target_link_libraries(example_async_tls hiredis_cluster hiredis ${SSL_LIBRARY} ${EVENT_LIBRARY})
  add_dependencies(example_async_tls generate_tls_configs)
endif()

# Tests using simulated redis node
add_executable(clusterclient clusterclient.c)
target_link_libraries(clusterclient hiredis_cluster hiredis ${SSL_LIBRARY})
add_executable(clusterclient_all_nodes clusterclient_all_nodes.c)
target_link_libraries(clusterclient_all_nodes hiredis_cluster hiredis ${SSL_LIBRARY})
add_executable(clusterclient_async clusterclient_async.c)
target_link_libraries(clusterclient_async hiredis_cluster hiredis ${SSL_LIBRARY} ${EVENT_LIBRARY})
add_test(NAME set-get-test
         COMMAND "${CMAKE_SOURCE_DIR}/tests/scripts/set-get-test.sh"
                 "$<TARGET_FILE:clusterclient>"
         WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/scripts/")
add_test(NAME set-get-test-async
         COMMAND "${CMAKE_SOURCE_DIR}/tests/scripts/set-get-test.sh"
         "$<TARGET_FILE:clusterclient_async>"
         WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/scripts/")
add_test(NAME ask-redirect-test
         COMMAND "${CMAKE_SOURCE_DIR}/tests/scripts/ask-redirect-test.sh"
         "$<TARGET_FILE:clusterclient>"
         WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/scripts/")
add_test(NAME ask-redirect-test-async
         COMMAND "${CMAKE_SOURCE_DIR}/tests/scripts/ask-redirect-test.sh"
                 "$<TARGET_FILE:clusterclient_async>"
         WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/scripts/")
add_test(NAME moved-redirect-test
         COMMAND "${CMAKE_SOURCE_DIR}/tests/scripts/moved-redirect-test.sh"
                 "$<TARGET_FILE:clusterclient>"
                 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/scripts/")
# The test "moved-redirect-test-async" below triggers a warning when running
# with santitizers. TODO: Fix this problem and uncomment the test case below.
# add_test(NAME moved-redirect-test-async
#          COMMAND "${CMAKE_SOURCE_DIR}/tests/scripts/moved-redirect-test.sh"
#          "$<TARGET_FILE:clusterclient_async>"
#          WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/scripts/")
add_test(NAME dbsize-to-all-nodes-test
         COMMAND "${CMAKE_SOURCE_DIR}/tests/scripts/dbsize-to-all-nodes-test.sh"
                 "$<TARGET_FILE:clusterclient_all_nodes>"
                 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/tests/scripts/")
