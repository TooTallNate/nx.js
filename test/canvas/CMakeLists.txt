cmake_minimum_required(VERSION 3.20)
project(nxjs-canvas-test C)

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

# Find system libraries via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(FREETYPE REQUIRED freetype2)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
pkg_check_modules(CAIRO_FT REQUIRED cairo-ft)
pkg_check_modules(PNG REQUIRED libpng)
pkg_check_modules(TURBOJPEG REQUIRED libturbojpeg)
pkg_check_modules(WEBP REQUIRED libwebp)

set(NX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../source")

add_executable(nxjs-canvas-test
  src/main.c
  src/stubs.c
  ${NX_SOURCE_DIR}/canvas.c
  ${NX_SOURCE_DIR}/dommatrix.c
  ${NX_SOURCE_DIR}/font.c
  ${NX_SOURCE_DIR}/image.c
)

# CRITICAL: Compat headers FIRST for angle-bracket includes (<switch.h>, etc.)
# Then source/ for the real nx.js headers (types.h, canvas.h, etc.)
# Then src/ so main.c and stubs.c can find the source/ headers via include path
target_include_directories(nxjs-canvas-test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
  ${NX_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CAIRO_INCLUDE_DIRS}
  ${FREETYPE_INCLUDE_DIRS}
  ${HARFBUZZ_INCLUDE_DIRS}
  ${CAIRO_FT_INCLUDE_DIRS}
  ${PNG_INCLUDE_DIRS}
  ${TURBOJPEG_INCLUDE_DIRS}
  ${WEBP_INCLUDE_DIRS}
)

target_link_directories(nxjs-canvas-test PRIVATE
  ${CAIRO_LIBRARY_DIRS}
  ${FREETYPE_LIBRARY_DIRS}
  ${HARFBUZZ_LIBRARY_DIRS}
  ${CAIRO_FT_LIBRARY_DIRS}
  ${PNG_LIBRARY_DIRS}
  ${TURBOJPEG_LIBRARY_DIRS}
  ${WEBP_LIBRARY_DIRS}
)

target_link_libraries(nxjs-canvas-test PRIVATE
  qjs
  ${CAIRO_LIBRARIES}
  ${FREETYPE_LIBRARIES}
  ${HARFBUZZ_LIBRARIES}
  ${CAIRO_FT_LIBRARIES}
  ${PNG_LIBRARIES}
  ${TURBOJPEG_LIBRARIES}
  ${WEBP_LIBRARIES}
  m
  pthread
)

# Suppress warnings from nx.js source files (they're not ours to fix)
set_source_files_properties(
  ${NX_SOURCE_DIR}/canvas.c
  ${NX_SOURCE_DIR}/dommatrix.c
  ${NX_SOURCE_DIR}/font.c
  ${NX_SOURCE_DIR}/image.c
  PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare -Wno-implicit-fallthrough -Wno-missing-field-initializers"
)

# Copy the JS bridge to the build directory
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/canvas_api.js
  ${CMAKE_CURRENT_BINARY_DIR}/canvas_api.js
  COPYONLY
)
