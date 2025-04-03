cmake_minimum_required(VERSION 3.23)
project(PackageTest CXX)

find_package(persistent_file_queue CONFIG REQUIRED)

add_executable(example src/example.cpp)
target_link_libraries(example persistent_file_queue::persistent_file_queue)
