cmake_minimum_required(VERSION 3.20)
project(nxjs-wasm-test C)

include(FetchContent)

# Fetch quickjs-ng (same version as canvas test)
FetchContent_Declare(
  quickjs
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG        v0.10.1
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(quickjs)

# Fetch wasm3
FetchContent_Declare(
  wasm3
  GIT_REPOSITORY https://github.com/wasm3/wasm3.git
  GIT_TAG        v0.5.0
)
set(BUILD_WASI "none" CACHE STRING "WASI implementation" FORCE)
FetchContent_MakeAvailable(wasm3)

set(NX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/../../source")

add_executable(nxjs-wasm-test
  src/main.c
  src/stubs.c
  ${NX_SOURCE_DIR}/wasm.c
)

# CRITICAL: wasm3 PUBLIC headers provide the real <wasm3.h> and <m3_env.h>.
# Compat headers provide stubs for <switch.h>, <cairo-ft.h>, <ft2build.h>,
# <mbedtls/*.h> — everything else that types.h pulls in.
# Order matters: wasm3 headers must be found BEFORE any compat stubs that
# might shadow them (the canvas test has a stub wasm3.h we must NOT pick up).
target_include_directories(nxjs-wasm-test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/compat
  ${NX_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nxjs-wasm-test PRIVATE
  qjs
  m3
  m
  pthread
)

# Suppress warnings from nx.js source files and include wasm3 compat shim.
# The compat shim redefines m3_Realloc to handle the 4-arg call in wasm.c
# (nx.js was written against an older wasm3 API with a label parameter).
set_source_files_properties(
  ${NX_SOURCE_DIR}/wasm.c
  PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers -include ${CMAKE_CURRENT_SOURCE_DIR}/src/compat/wasm_compat.h"
)

# Copy the JS bridge to the build directory
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/wasm_api.js
  ${CMAKE_CURRENT_BINARY_DIR}/wasm_api.js
  COPYONLY
)
