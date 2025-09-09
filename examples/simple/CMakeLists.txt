cmake_minimum_required(VERSION 3.15)
project(tribune_simple_example)

# Find the tribune library
set(TRIBUNE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
find_path(TRIBUNE_INCLUDE_DIR 
    NAMES "server/tribune_server.hpp"
    PATHS "${TRIBUNE_ROOT}/include"
)

find_library(TRIBUNE_LIB 
    NAMES tribune_lib
    PATHS "${TRIBUNE_ROOT}"
)

if(NOT TRIBUNE_INCLUDE_DIR OR NOT TRIBUNE_LIB)
    message(FATAL_ERROR "Tribune library not found. Build the main project first with 'cmake . && make' from the root directory.")
endif()

# Add httplib include path
include_directories("${TRIBUNE_ROOT}/_deps/httplib-src")

# Add nlohmann json include path  
include_directories("${TRIBUNE_ROOT}/_deps/nlohmann_json-src/include")

# Add stduuid include path
include_directories("${TRIBUNE_ROOT}/_deps/stduuid-src/include")

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Disable OpenSSL support for simple example
# add_definitions(-DCPPHTTPLIB_OPENSSL_SUPPORT)

# Find libsodium
find_path(SODIUM_INCLUDE_DIR sodium.h)
find_library(SODIUM_LIBRARY sodium)

if(NOT SODIUM_INCLUDE_DIR OR NOT SODIUM_LIBRARY)
    message(FATAL_ERROR "libsodium not found")
endif()

# OpenSSL disabled for simple example
# find_package(OpenSSL REQUIRED)

# Include directories
include_directories(${TRIBUNE_INCLUDE_DIR})
include_directories(${SODIUM_INCLUDE_DIR})

# Build server executable
add_executable(simple_server server.cpp)
target_link_libraries(simple_server 
    ${TRIBUNE_LIB}
    ${SODIUM_LIBRARY}
    pthread
)

# Build client executable  
add_executable(simple_client client.cpp)
target_link_libraries(simple_client 
    ${TRIBUNE_LIB}
    ${SODIUM_LIBRARY}
    pthread
)