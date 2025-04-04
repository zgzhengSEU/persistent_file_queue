cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(persistent_queue_benchmark)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必要的包
find_package(benchmark CONFIG REQUIRED)

# 添加性能测试可执行文件
add_executable(benchmark_persistent_queue src/benchmark_persistent_queue.cpp)
target_link_libraries(benchmark_persistent_queue 
    benchmark::benchmark
    persistent_file_queue
)