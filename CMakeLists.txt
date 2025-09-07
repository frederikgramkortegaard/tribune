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

FetchContent_Declare(
  stduuid
  GIT_REPOSITORY https://github.com/mariusbancila/stduuid.git
  GIT_TAG        v1.2.3
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

set(FETCHCONTENT_QUIET FALSE)
FetchContent_MakeAvailable(httplib nlohmann_json stduuid)

# Include directories
include_directories(include)
include_directories(${SODIUM_INCLUDE_DIR})

# Collect source files
file(GLOB_RECURSE TRIBUNE_SOURCES 
    src/*.cpp
)

# Create tribune library
add_library(tribune_lib STATIC ${TRIBUNE_SOURCES})

# Link dependencies
target_link_libraries(tribune_lib 
    PUBLIC 
        httplib::httplib
        nlohmann_json::nlohmann_json
        stduuid
        ${SODIUM_LIBRARY}
)

# Set include directories for the library
target_include_directories(tribune_lib 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SODIUM_INCLUDE_DIR}
)

# Optional: Create a simple test executable
option(BUILD_EXAMPLES "Build example executables" OFF)

if(BUILD_EXAMPLES)
    # Create a simple example executable to test compilation
    add_executable(tribune_test_build examples/test_build.cpp)
    target_link_libraries(tribune_test_build tribune_lib)
endif()
