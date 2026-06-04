cmake_minimum_required(VERSION 3.20)
project(nxjs-test CXX)

# ============================================================================
# nxjs-test — host-platform conformance test binary (V8 + libuv + Skia era).
#
# Links against the HOST-native V8, Skia, and libuv built into the
# pacman-packages image under /opt/host (see that repo's Dockerfile). The
# Switch (target_os=horizon) portlibs cannot link into a host ELF, so a
# dedicated host toolchain is required.
#
# Toolchain: the host V8 and Skia in the image are built against the system
# libstdc++ (g++ 12), so the harness must use libstdc++ too — V8's public
# headers pass std:: types across the ABI boundary, which must match the
# archives. A C++20-capable compiler is required (g++ 12 or clang-16+).
# ============================================================================

# /opt/host layout from the pacman-packages image.
set(HOST_PREFIX "/opt/host" CACHE PATH "Host-native V8/Skia/libuv/ada prefix")
set(V8_DIR    "${HOST_PREFIX}/v8")
set(SKIA_DIR  "${HOST_PREFIX}/skia")
set(LIBUV_DIR "${HOST_PREFIX}/libuv")
set(ADA_DIR   "${HOST_PREFIX}/ada")

find_package(PkgConfig REQUIRED)
# Leaf deps come from apt (host ABI); only the engine/renderer/loop are pinned.
pkg_check_modules(FREETYPE REQUIRED freetype2)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
pkg_check_modules(PNG REQUIRED libpng)
pkg_check_modules(TURBOJPEG REQUIRED libturbojpeg)
pkg_check_modules(WEBP REQUIRED libwebp)
pkg_check_modules(ZLIB REQUIRED zlib)
pkg_check_modules(ZSTD REQUIRED libzstd)

# ============================================================================
# Source files
# ============================================================================

set(NX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../../source")

set(TEST_SOURCES
  src/main.cc
  src/stubs.cc
)

# Real nx.js source modules that are portable (compiled from source/*.cc, with
# libnx stubbed by compat/). skia_gpu.cc is EXCLUDED — it's the GPU (EGL/Ganesh)
# screen provider, which the host raster harness does not use.
set(NX_SOURCES
  ${NX_SOURCE_DIR}/async.cc
  ${NX_SOURCE_DIR}/canvas.cc
  ${NX_SOURCE_DIR}/canvas_path.cc
  ${NX_SOURCE_DIR}/compression.cc
  ${NX_SOURCE_DIR}/crypto.cc
  ${NX_SOURCE_DIR}/dns.cc
  ${NX_SOURCE_DIR}/dommatrix.cc
  ${NX_SOURCE_DIR}/error.cc
  ${NX_SOURCE_DIR}/font.cc
  ${NX_SOURCE_DIR}/fs.cc
  ${NX_SOURCE_DIR}/image.cc
  ${NX_SOURCE_DIR}/path2d.cc
  ${NX_SOURCE_DIR}/tcp.cc
  ${NX_SOURCE_DIR}/tls.cc
  ${NX_SOURCE_DIR}/udp.cc
  ${NX_SOURCE_DIR}/url.cc
  ${NX_SOURCE_DIR}/util.cc
  ${NX_SOURCE_DIR}/window.cc
  ${NX_SOURCE_DIR}/wrap.cc
)

add_executable(nxjs-test ${TEST_SOURCES} ${NX_SOURCES})

# ============================================================================
# Includes
# ============================================================================

# harfbuzz: canvas.h does #include <harfbuzz/hb.h>; pkg-config points at
# .../include/harfbuzz, so add its parent too.
set(HARFBUZZ_PARENT_INCLUDE "")
foreach(dir ${HARFBUZZ_INCLUDE_DIRS})
  get_filename_component(parent "${dir}" DIRECTORY)
  list(APPEND HARFBUZZ_PARENT_INCLUDE "${parent}")
endforeach()
list(REMOVE_DUPLICATES HARFBUZZ_PARENT_INCLUDE)

# Order matters: compat/ first (provides <switch.h> + libnx stubs), then the
# real source/ headers, then src/.
target_include_directories(nxjs-test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
  ${NX_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${V8_DIR}/include
  ${SKIA_DIR}
  ${LIBUV_DIR}/include
  ${ADA_DIR}/include
  ${FREETYPE_INCLUDE_DIRS}
  ${HARFBUZZ_INCLUDE_DIRS}
  ${HARFBUZZ_PARENT_INCLUDE}
  ${PNG_INCLUDE_DIRS}
  ${TURBOJPEG_INCLUDE_DIRS}
  ${WEBP_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
  ${ZSTD_INCLUDE_DIRS}
)

target_link_directories(nxjs-test PRIVATE
  ${FREETYPE_LIBRARY_DIRS}
  ${HARFBUZZ_LIBRARY_DIRS}
  ${PNG_LIBRARY_DIRS}
  ${TURBOJPEG_LIBRARY_DIRS}
  ${WEBP_LIBRARY_DIRS}
  ${ZLIB_LIBRARY_DIRS}
  ${ZSTD_LIBRARY_DIRS}
)

# ============================================================================
# Link libraries
# ============================================================================
# V8 monolith links in a group (circular refs), like the device Makefile.
target_link_libraries(nxjs-test PRIVATE
  -Wl,--start-group
  ${V8_DIR}/lib/libv8_monolith.a
  ${V8_DIR}/lib/libabsl.a
  ${V8_DIR}/lib/libchrome_zlib.a
  ${V8_DIR}/lib/libcompression_utils_portable.a
  -Wl,--end-group
  ${SKIA_DIR}/lib/libskia.a
  ${SKIA_DIR}/lib/libskshaper.a
  ${SKIA_DIR}/lib/libskunicode_core.a
  ${SKIA_DIR}/lib/libskunicode_libgrapheme.a
  ${SKIA_DIR}/lib/libskcms.a
  ${LIBUV_DIR}/lib/libuv.a
  ${ADA_DIR}/lib/libada.a
  mbedtls mbedx509 mbedcrypto
  ${FREETYPE_LIBRARIES}
  ${HARFBUZZ_LIBRARIES}
  ${TURBOJPEG_LIBRARIES}
  # Skia's codecs need the full libjpeg API (jpeg_std_error etc., distinct from
  # turbojpeg's tj*) and libwebpdemux (animated/demux), beyond what nx.js's own
  # decoders use.
  jpeg
  ${WEBP_LIBRARIES}
  webpdemux
  ${PNG_LIBRARIES}
  ${ZLIB_LIBRARIES}
  ${ZSTD_LIBRARIES}
  m pthread dl
)

# ============================================================================
# Compiler / linker flags
# ============================================================================
# Use the system libstdc++ (the default), matching host V8 + Skia which are
# built against libstdc++ too — one consistent std:: ABI across all three.
#
# Link with lld: V8/Skia archives are produced by LLVM tooling, and GNU bfd ld
# chokes on them ("unknown architecture of input file ... incompatible with
# i386:x86-64"). lld reads them fine.
target_link_options(nxjs-test PRIVATE -fuse-ld=lld)

# Quiet warnings in the ported nx.js source (compiled with libnx stubbed).
set_source_files_properties(${NX_SOURCES}
  PROPERTIES COMPILE_FLAGS
  "-Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough -Wno-missing-field-initializers"
)

set_property(TARGET nxjs-test PROPERTY CXX_STANDARD 20)
set_property(TARGET nxjs-test PROPERTY CXX_STANDARD_REQUIRED ON)
