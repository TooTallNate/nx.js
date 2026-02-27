cmake_minimum_required(VERSION 3.20)
project(nxjs-runtime-test C)

include(FetchContent)

# Fetch quickjs-ng
FetchContent_Declare(
  quickjs
  GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
  GIT_TAG        v0.12.1
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(QJS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(quickjs)

add_executable(nxjs-runtime-test
  src/main.c
)

target_include_directories(nxjs-runtime-test PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nxjs-runtime-test PRIVATE
  qjs
  m
  pthread
)
