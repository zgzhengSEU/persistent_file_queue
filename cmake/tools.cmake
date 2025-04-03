# this file contains a list of tools that can be activated and downloaded on-demand each tool is
# enabled during configuration by passing an additional `-DUSE_<TOOL>=<VALUE>` argument to CMake

# only activate tools for top level project
if(NOT PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  return()
endif()