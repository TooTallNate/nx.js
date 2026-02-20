cmake_minimum_required(VERSION 3.20)
project(nxjs-webcrypto-test C)

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

# Fetch mbedtls for crypto primitives
FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG        v3.6.0
)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)

set(NX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../source")

add_executable(nxjs-webcrypto-test
  src/main.c
  src/stubs.c
  ${NX_SOURCE_DIR}/crypto.c
)

# Compat headers must come first so our switch.h / wasm3.h / etc. are found
# before any real system headers with the same name.
target_include_directories(nxjs-webcrypto-test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
  ${NX_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nxjs-webcrypto-test PRIVATE
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
  PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers"
)

# Copy shared test helpers into the build directory
configure_file(
  ${CMAKE_SOURCE_DIR}/../shared/test-helpers.js
  ${CMAKE_CURRENT_BINARY_DIR}/test-helpers.js
  COPYONLY
)
