cmake_minimum_required(VERSION 3.14)
project(tribune)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

FetchContent_MakeAvailable(httplib nlohmann_json)

# Include directories
include_directories(include)

# Example applications
add_executable(server_app apps/example/server_app.cpp src/server/tribune_server.cpp src/server/client_state.cpp src/protocol/parser.cpp src/mpc/sum_computation.cpp src/crypto/signature.cpp)
target_link_libraries(server_app PRIVATE httplib::httplib nlohmann_json::nlohmann_json)

add_executable(client_app apps/example/client_app.cpp src/client/tribune_client.cpp src/client/data_collection_module.cpp src/protocol/parser.cpp src/mpc/sum_computation.cpp src/crypto/signature.cpp)
target_link_libraries(client_app PRIVATE httplib::httplib nlohmann_json::nlohmann_json)