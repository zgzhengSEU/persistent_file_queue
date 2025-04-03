cmake_minimum_required(VERSION 3.23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

message(STATUS "Building PersistentFileQueue")

# ---- Project ----
# Note: update this to your new project's name and version
project(
  persistent_file_queue
  VERSION 1.0
  LANGUAGES CXX
)

# ---- Include guards ----
if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Add dependencies via Conan ----
# see https://conan.org.cn/ for more info
find_package(spdlog CONFIG REQUIRED)

# ---- Add source files ----

# Note: globbing sources is considered bad practice as CMake's generators may not detect new files
# automatically. Keep that in mind when changing files, or explicitly mention them here.
file(GLOB_RECURSE public_headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
message(STATUS "Found ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h files: ${public_headers}")
message(STATUS "Found ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp files: ${sources}")

# ---- Create library ----
# Note: for header-only libraries change all PUBLIC flags to INTERFACE and create an interface
# target: add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME} ${public_headers} ${sources})
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(${PROJECT_NAME} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

# Include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)

# Link dependencies
target_link_libraries(${PROJECT_NAME} fmt::fmt spdlog::spdlog)

# ---- Add tests ----
if(NOT BUILD_TESTING STREQUAL OFF)
  message(STATUS "Building tests")
  add_subdirectory(tests)
endif()

# ---- Create an installable target ----
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${public_headers})
install(TARGETS ${PROJECT_NAME}
        PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}"
        RUNTIME DESTINATION bin
)
message(STATUS "Installing ${PROJECT_NAME} to ${CMAKE_INSTALL_INCLUDEDIR}")