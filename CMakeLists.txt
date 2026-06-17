cmake_minimum_required(VERSION 3.20)

project(RedisClone
    VERSION 1.0.0
    DESCRIPTION "A small Redis-inspired key-value store in C++20"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(REDISCLONE_BUILD_TESTS "Build RedisClone unit tests" ON)

find_package(Threads REQUIRED)

add_library(redisclone_core
    src/command_executor.cpp
    src/command_parser.cpp
    src/datastore.cpp
    src/server.cpp)

target_include_directories(redisclone_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(redisclone_core
    PUBLIC
        Threads::Threads)

target_compile_options(redisclone_core
    PRIVATE
        $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra -Wpedantic>)

add_executable(redisclone
    src/main.cpp)

target_link_libraries(redisclone
    PRIVATE
        redisclone_core)

add_executable(redisclone_benchmark
    benchmark/benchmark.cpp)

target_link_libraries(redisclone_benchmark
    PRIVATE
        redisclone_core)

if(REDISCLONE_BUILD_TESTS)
    enable_testing()

    add_executable(redisclone_tests
        tests/test_runner.cpp)

    target_link_libraries(redisclone_tests
        PRIVATE
            redisclone_core)

    add_test(NAME redisclone_tests COMMAND redisclone_tests)
endif()

