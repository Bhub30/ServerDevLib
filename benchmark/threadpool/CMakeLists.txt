project(
    ThreadPoolBenchmark
    VERSION 1.0
    LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
    nanobench
    GIT_REPOSITORY https://github.com/martinus/nanobench.git
    GIT_TAG v4.1.0
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(nanobench)

include_directories("${CMAKE_SOURCE_DIR}/include/server/threadpool")

add_executable(${PROJECT_NAME} ThreadPoolBenchmark.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE nanobench threadpool)
