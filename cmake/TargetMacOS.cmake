include(ExternalProject)

if(NOT CMAKE_CXX_COMPILER MATCHES "clang")
  message(WARNING "macOS build expects CMAKE_CXX_COMPILER=clang++, set with -DCMAKE_CXX_COMPILER=clang++")
endif()

set(BUILD_JOBS 4 CACHE STRING "Parallel build jobs for external dependencies")

set(PLATFORM_NAME "macos")
