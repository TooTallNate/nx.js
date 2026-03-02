cmake_minimum_required(VERSION 3.20)
project(nxjs-test C CXX)

include(FetchContent)

# ============================================================================
# Fetch third-party dependencies
# ============================================================================

# QuickJS-ng — JavaScript engine
FetchContent_Declare(
  quickjs
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG        v0.12.1
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(quickjs)

# mbedtls — TLS, crypto, and certificate handling
FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG        v3.6.0
)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(mbedtls)

# wasm3 — WebAssembly interpreter
FetchContent_Declare(
  wasm3
  GIT_REPOSITORY https://github.com/wasm3/wasm3.git
  GIT_TAG        7ed176d98586bf22aaa6db3b2d379af4c3cb0655
)
set(BUILD_WASI "none" CACHE STRING "WASI implementation" FORCE)
FetchContent_MakeAvailable(wasm3)

# ============================================================================
# Find system libraries via pkg-config
# ============================================================================

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(FREETYPE REQUIRED freetype2)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
pkg_check_modules(CAIRO_FT REQUIRED cairo-ft)
pkg_check_modules(PNG REQUIRED libpng)
pkg_check_modules(TURBOJPEG REQUIRED libturbojpeg)
pkg_check_modules(WEBP REQUIRED libwebp)
pkg_check_modules(ZLIB REQUIRED zlib)
pkg_check_modules(ZSTD REQUIRED libzstd)

# ============================================================================
# Source file lists
# ============================================================================

set(NX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../../source")

# Test harness sources
set(TEST_SOURCES
  src/main.c
  src/stubs.c
)

# Real nx.js source files (portable modules)
set(NX_SOURCES
  ${NX_SOURCE_DIR}/async.c
  ${NX_SOURCE_DIR}/canvas.c
  ${NX_SOURCE_DIR}/compression.c
  ${NX_SOURCE_DIR}/crypto.c
  ${NX_SOURCE_DIR}/dns.c
  ${NX_SOURCE_DIR}/dommatrix.c
  ${NX_SOURCE_DIR}/error.c
  ${NX_SOURCE_DIR}/font.c
  ${NX_SOURCE_DIR}/fs.c
  ${NX_SOURCE_DIR}/image.c
  ${NX_SOURCE_DIR}/poll.c
  ${NX_SOURCE_DIR}/tcp.c
  ${NX_SOURCE_DIR}/thpool.c
  ${NX_SOURCE_DIR}/tls.c
  ${NX_SOURCE_DIR}/udp.c
  ${NX_SOURCE_DIR}/uint8array.c
  ${NX_SOURCE_DIR}/url.c
  ${NX_SOURCE_DIR}/util.c
  ${NX_SOURCE_DIR}/wasm.c
  ${NX_SOURCE_DIR}/web.c
  ${NX_SOURCE_DIR}/window.c
)

# ada URL parser (C++ — single-header library)
set(NX_CXX_SOURCES
  ${NX_SOURCE_DIR}/ada.cpp
)

# ============================================================================
# Build the test binary
# ============================================================================

add_executable(nxjs-test
  ${TEST_SOURCES}
  ${NX_SOURCES}
  ${NX_CXX_SOURCES}
)

# ============================================================================
# Include directories
# ============================================================================

# canvas.h does #include <harfbuzz/hb.h> so we need the parent of the
# harfbuzz include directory (pkg-config gives us .../include/harfbuzz,
# but we need .../include so that harfbuzz/hb.h resolves correctly)
set(HARFBUZZ_PARENT_INCLUDE "")
foreach(dir ${HARFBUZZ_INCLUDE_DIRS})
  get_filename_component(parent "${dir}" DIRECTORY)
  list(APPEND HARFBUZZ_PARENT_INCLUDE "${parent}")
endforeach()
list(REMOVE_DUPLICATES HARFBUZZ_PARENT_INCLUDE)

# CRITICAL: Compat headers FIRST for angle-bracket includes (<switch.h>, etc.)
# Then source/ for the real nx.js headers (types.h, canvas.h, etc.)
# Then src/ so main.c and stubs.c can find the source/ headers via include path
target_include_directories(nxjs-test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
  ${NX_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CAIRO_INCLUDE_DIRS}
  ${FREETYPE_INCLUDE_DIRS}
  ${HARFBUZZ_INCLUDE_DIRS}
  ${HARFBUZZ_PARENT_INCLUDE}
  ${CAIRO_FT_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIRS}
  ${TURBOJPEG_INCLUDE_DIRS}
  ${WEBP_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
  ${ZSTD_INCLUDE_DIRS}
  ${mbedtls_SOURCE_DIR}/include
)

# ============================================================================
# Link directories
# ============================================================================

target_link_directories(nxjs-test PRIVATE
  ${CAIRO_LIBRARY_DIRS}
  ${FREETYPE_LIBRARY_DIRS}
  ${HARFBUZZ_LIBRARY_DIRS}
  ${CAIRO_FT_LIBRARY_DIRS}
  ${PNG_LIBRARY_DIRS}
  ${TURBOJPEG_LIBRARY_DIRS}
  ${WEBP_LIBRARY_DIRS}
  ${ZLIB_LIBRARY_DIRS}
  ${ZSTD_LIBRARY_DIRS}
)

# ============================================================================
# Link libraries
# ============================================================================

# Mirrors the production Makefile LIBS line, minus libnx:
# -pthread -lmbedtls -lmbedx509 -lmbedcrypto -lharfbuzz
# freetype2 cairo (via pkg-config)
# -lturbojpeg -lwebp -lqjs -lm3 -lm -lzstd
target_link_libraries(nxjs-test PRIVATE
  qjs
  m3
  mbedtls
  mbedx509
  mbedcrypto
  ${CAIRO_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  ${HARFBUZZ_LIBRARIES}
  ${CAIRO_FT_LIBRARIES}
  ${PNG_LIBRARIES}
  ${TURBOJPEG_LIBRARIES}
  ${WEBP_LIBRARIES}
  ${ZLIB_LIBRARIES}
  ${ZSTD_LIBRARIES}
  m
  pthread
)

# ============================================================================
# Compiler flags
# ============================================================================

# Suppress warnings from nx.js source files (they're not ours to fix)
# -Wno-implicit-function-declaration is needed because some source files
# (e.g. tcp.c) don't include <unistd.h> for close() — it's provided by
# libnx headers on Switch but not on the host.
set_source_files_properties(${NX_SOURCES}
  PROPERTIES COMPILE_FLAGS
  "-Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough -Wno-missing-field-initializers -Wno-implicit-function-declaration -Wno-incompatible-pointer-types -Wno-tautological-constant-out-of-range-compare -Wno-absolute-value -Wno-format-security -Wno-int-conversion"
)

set_source_files_properties(${NX_CXX_SOURCES}
  PROPERTIES COMPILE_FLAGS
  "-Wno-unused-parameter -Wno-sign-compare"
)

# C standard
set_property(TARGET nxjs-test PROPERTY C_STANDARD 11)
set_property(TARGET nxjs-test PROPERTY CXX_STANDARD 17)


