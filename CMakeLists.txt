cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(MyProject VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(ratelimiter_example examples/ratelimiter_example.cpp)

# GoogleTest (fetched at configure time)
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Unit tests
enable_testing()
add_executable(unit_tests tests/ratelimiter_test.cpp)
target_link_libraries(unit_tests PRIVATE GTest::gtest_main)
# target_compile_definitions(unit_tests PRIVATE RATELIMITER_DEBUG)

include(GoogleTest)
gtest_discover_tests(unit_tests)
