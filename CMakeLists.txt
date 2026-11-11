cmake_minimum_required(VERSION 3.20)

project(Melodia LANGUAGES CXX)

# Read version from main.cpp
file(STRINGS "${CMAKE_SOURCE_DIR}/src/main.cpp" VERSION_LINE REGEX "#define[ \t]+VERSION[ \t]+\"[^\"]*\"")
string(REGEX REPLACE ".*#define[ \t]+VERSION[ \t]+\"([^\"]*)\".*" "\\1" MAIN_CPP_VERSION_STRING "${VERSION_LINE}")

set(APP_VERSION "${MAIN_CPP_VERSION_STRING}" CACHE STRING "Version string (default read from main.cpp).")
option(ARCHIVE_DIST "Package the dist output as an archive instead of a plain directory" OFF)
option(COPY_BIN "When archiving, also drop a standalone copy of the binary next to the archive" OFF)
option(SANITIZE "Enable ASan" OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PROJECT_DISPLAY_NAME "melodiad")
set(TEMP_DIST_DIR "${CMAKE_BINARY_DIR}/dist-tmp")

file(GLOB APP_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

include(cmake/ExternalLibs.cmake)
melodia_fetch_external_libs("${EXTERNAL_LIBS}")

add_executable(${PROJECT_DISPLAY_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_DISPLAY_NAME} PRIVATE
  "${CMAKE_SOURCE_DIR}/src/include"
  # "external/lib"
)
target_compile_definitions(${PROJECT_DISPLAY_NAME} PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_options(${PROJECT_DISPLAY_NAME} PRIVATE -fdiagnostics-color)

if(CMAKE_BUILD_TYPE STREQUAL "Debug") # Debug build
  target_compile_options(${PROJECT_DISPLAY_NAME} PRIVATE
    -g -O0 -Wall -Wextra -Werror -Wfatal-errors
  )
  set(OUT_DIR "${CMAKE_SOURCE_DIR}/dist/debug")
else() # Release build
  target_compile_options(${PROJECT_DISPLAY_NAME} PRIVATE -O2)
  set(OUT_DIR "${CMAKE_SOURCE_DIR}/dist/release")
endif()

if(SANITIZE) # Use sanitization
  target_compile_options(${PROJECT_DISPLAY_NAME} PRIVATE
    -fsanitize=address -fsanitize=undefined
    -fno-omit-frame-pointer -fno-sanitize-recover=all
  )
  target_link_options(${PROJECT_DISPLAY_NAME} PRIVATE
    -fsanitize=address -fsanitize=undefined
  )
endif()

# Base libs
target_link_libraries(${PROJECT_DISPLAY_NAME} PRIVATE ssl crypto)

# Platform specific config
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND MINGW)
    include(cmake/TargetWindows.cmake)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    include(cmake/TargetMacOS.cmake)
else()
    include(cmake/TargetLinux.cmake)
endif()
