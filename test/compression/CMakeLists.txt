cmake_minimum_required(VERSION 3.20)
project(nxjs-compression-test C)

include(FetchContent)

# Fetch quickjs-ng (same version as canvas/wasm tests)
FetchContent_Declare(
  quickjs
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG        v0.10.1
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(quickjs)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZLIB REQUIRED zlib)
pkg_check_modules(ZSTD REQUIRED libzstd)

set(NX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../source")

add_executable(nxjs-compression-test
  src/main.c
  src/stubs.c
  ${NX_SOURCE_DIR}/compression.c
  ${NX_SOURCE_DIR}/util.c
)

# Compat headers first (for <switch.h>, <wasm3.h>, <cairo-ft.h>, etc.),
# then source dir for types.h, compression.h, util.h, async.h
target_include_directories(nxjs-compression-test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
  ${NX_SOURCE_DIR}
  ${ZLIB_INCLUDE_DIRS}
  ${ZSTD_INCLUDE_DIRS}
)

target_link_directories(nxjs-compression-test PRIVATE
  ${ZLIB_LIBRARY_DIRS}
  ${ZSTD_LIBRARY_DIRS}
)

target_link_libraries(nxjs-compression-test PRIVATE
  qjs
  ${ZLIB_LIBRARIES}
  ${ZSTD_LIBRARIES}
  m
  pthread
)

# Suppress warnings from nx.js source files
set_source_files_properties(
  ${NX_SOURCE_DIR}/compression.c
  ${NX_SOURCE_DIR}/util.c
  PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare"
)

# Copy shared test helpers into the build directory
configure_file(
  ${CMAKE_SOURCE_DIR}/../shared/test-helpers.js
  ${CMAKE_CURRENT_BINARY_DIR}/test-helpers.js
  COPYONLY
)
