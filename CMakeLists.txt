cmake_minimum_required(VERSION 3.14)
project(tribune)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug build flag
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_definitions(DEBUG_BUILD)
    message(STATUS "Debug logging enabled")
endif()

# Fetch dependencies
include(FetchContent)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.15.3
)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)

# Find libsodium (homebrew installation on macOS)
find_path(SODIUM_INCLUDE_DIR sodium.h 
    PATHS /opt/homebrew/include /usr/local/include
    PATH_SUFFIXES sodium)
find_library(SODIUM_LIBRARY 
    NAMES sodium libsodium
    PATHS /opt/homebrew/lib /usr/local/lib)

if(SODIUM_INCLUDE_DIR AND SODIUM_LIBRARY)
    message(STATUS "Found libsodium: ${SODIUM_LIBRARY}")
else()
    message(FATAL_ERROR "libsodium not found")
endif()

FetchContent_MakeAvailable(httplib nlohmann_json)

# Include directories
include_directories(include)

# Example applications
add_executable(server_app apps/example/server_app.cpp src/server/tribune_server.cpp src/server/client_state.cpp src/protocol/parser.cpp src/mpc/sum_computation.cpp src/crypto/signature.cpp)
target_include_directories(server_app PRIVATE ${SODIUM_INCLUDE_DIR})
target_link_libraries(server_app PRIVATE httplib::httplib nlohmann_json::nlohmann_json ${SODIUM_LIBRARY})

add_executable(client_app apps/example/client_app.cpp src/client/tribune_client.cpp apps/example/src/data_collection_module.cpp src/protocol/parser.cpp src/mpc/sum_computation.cpp src/crypto/signature.cpp)
target_include_directories(client_app PRIVATE apps/example ${SODIUM_INCLUDE_DIR})
target_link_libraries(client_app PRIVATE httplib::httplib nlohmann_json::nlohmann_json ${SODIUM_LIBRARY})

# Federated Machine Learning application
add_executable(federated_server_app 
    apps/federated-machinelearning/server_app.cpp 
    src/server/tribune_server.cpp 
    src/server/client_state.cpp 
    src/protocol/parser.cpp 
    apps/federated-machinelearning/src/federated_computation.cpp
    src/crypto/signature.cpp)
target_include_directories(federated_server_app PRIVATE apps/federated-machinelearning ${SODIUM_INCLUDE_DIR})
target_link_libraries(federated_server_app PRIVATE httplib::httplib nlohmann_json::nlohmann_json ${SODIUM_LIBRARY})

add_executable(federated_client_app 
    apps/federated-machinelearning/client_app.cpp 
    src/client/tribune_client.cpp 
    apps/federated-machinelearning/src/federated_logout_module.cpp
    apps/federated-machinelearning/src/federated_computation.cpp
    src/protocol/parser.cpp 
    src/crypto/signature.cpp)
target_include_directories(federated_client_app PRIVATE apps/federated-machinelearning ${SODIUM_INCLUDE_DIR})
target_link_libraries(federated_client_app PRIVATE httplib::httplib nlohmann_json::nlohmann_json ${SODIUM_LIBRARY})