cmake_minimum_required(VERSION 3.20)
project(nxjs-crypto-test C)

include(FetchContent)

# Fetch quickjs-ng
FetchContent_Declare(
  quickjs
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG        v0.10.1
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(quickjs)

# Fetch mbedtls (real crypto library)
FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG        v3.6.0
)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)

set(NX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../source")

add_executable(nxjs-crypto-test
  src/main.c
  src/stubs.c
  ${NX_SOURCE_DIR}/crypto.c
  ${NX_SOURCE_DIR}/uint8array.c
  ${NX_SOURCE_DIR}/util.c
)

# Compat headers (switch.h, poll.h, etc.) must come BEFORE system includes
# so our stubs are found. Real mbedtls headers come from the fetched library.
target_include_directories(nxjs-crypto-test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
  ${NX_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nxjs-crypto-test PRIVATE
  qjs
  mbedtls
  mbedx509
  mbedcrypto
  m
  pthread
)

# Suppress warnings from nx.js source files
set_source_files_properties(
  ${NX_SOURCE_DIR}/crypto.c
  ${NX_SOURCE_DIR}/uint8array.c
  ${NX_SOURCE_DIR}/util.c
  PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers"
)

# Copy shared test helpers
configure_file(
  ${CMAKE_SOURCE_DIR}/../shared/test-helpers.js
  ${CMAKE_CURRENT_BINARY_DIR}/test-helpers.js
  COPYONLY
)
