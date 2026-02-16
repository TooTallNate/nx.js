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

# Fetch wasm3 — pinned to the same commit used in nx.js's pacman PKGBUILD:
# https://github.com/TooTallNate/pacman-packages/blob/nxjs/switch/wasm3/PKGBUILD
FetchContent_Declare(
  wasm3
  GIT_REPOSITORY https://github.com/wasm3/wasm3.git
  GIT_TAG        7ed176d98586bf22aaa6db3b2d379af4c3cb0655
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

# Suppress warnings from nx.js source files.
# No compat shim needed — we pin the exact wasm3 commit that nx.js targets,
# so struct layouts and API signatures (including 4-arg m3_Realloc) match.
set_source_files_properties(
  ${NX_SOURCE_DIR}/wasm.c
  PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-sign-compare -Wno-missing-field-initializers"
)

# NOTE: The JS bridge (wasm_api.js) is now built by esbuild from the real
# runtime wasm.ts — see build-bridge.mjs. Run `node build-bridge.mjs` after
# cmake --build to produce build/wasm_api.js.
