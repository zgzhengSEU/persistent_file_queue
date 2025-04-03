cmake_minimum_required(VERSION 3.23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(test_persistent_file_queue LANGUAGES CXX)

# ---- Options ----
option(ENABLE_TEST_COVERAGE "Enable test coverage" OFF)

# --- Import tools ----
include(../cmake/tools.cmake)

# ---- Dependencies ----
find_package(GTest CONFIG REQUIRED)

# ---- Create binary ----
file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc)
add_executable(${PROJECT_NAME} ${sources})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

# Link dependencies
target_link_libraries(${PROJECT_NAME} GTest::gtest GTest::gtest_main persistent_file_queue)

# enable compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  target_compile_options(persistent_file_queue PUBLIC -Wall -Wpedantic -Wextra -Werror)
elseif(MSVC)
  target_compile_options(persistent_file_queue PUBLIC /W4 /WX)
  target_compile_definitions(${PROJECT_NAME} PUBLIC DOCTEST_CONFIG_USE_STD_HEADERS)
endif()

# ---- code coverage ----
if(ENABLE_TEST_COVERAGE)
  target_compile_options(persistent_file_queue PUBLIC -O0 -g -fprofile-arcs -ftest-coverage)
  target_link_options(persistent_file_queue PUBLIC -fprofile-arcs -ftest-coverage)
endif()
