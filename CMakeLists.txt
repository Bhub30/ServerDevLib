cmake_minimum_required(VERSION 3.20)
project(ServerDevLib)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# add_subdirectory("benchmark/threadpool")
add_subdirectory("include/server")
add_subdirectory("example")
