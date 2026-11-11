include(ExternalProject)

set(BUILD_JOBS 4 CACHE STRING "Parallel build jobs for external dependencies")
set(DEPS_ROOT "${CMAKE_SOURCE_DIR}/external/build")
set(DEPS_BUILD_DIR "${DEPS_ROOT}/build-mingw-deps")

set(MINGW_BASE_CMAKE_ARGS
  -DCMAKE_SYSTEM_NAME=Windows
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++
  -DBUILD_SHARED_LIBS=FALSE
  -DBUILD_TESTING=OFF
)

target_link_libraries(${PROJECT_DISPLAY_NAME} PRIVATE
    ssl crypto
    stdc++exp
    ws2_32 crypt32 bcrypt iphlpapi
)
target_link_options(${PROJECT_DISPLAY_NAME} PRIVATE -static -static-libgcc -static-libstdc++)

target_compile_options(${PROJECT_DISPLAY_NAME} PRIVATE -fpermissive)

set_target_properties(${PROJECT_DISPLAY_NAME} PROPERTIES
    SUFFIX ".exe"
)

set(PLATFORM_NAME "windows")

