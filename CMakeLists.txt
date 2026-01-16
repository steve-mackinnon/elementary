cmake_minimum_required(VERSION 3.15)
project(elementary VERSION 0.11.5)


option(ONLY_BUILD_WASM "Only build the wasm subdirectory" OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(BUILD_TESTS "Build unit tests" OFF)

if(${ONLY_BUILD_WASM})
  add_subdirectory(runtime)
  add_subdirectory(wasm)
else()
  add_subdirectory(runtime)
  add_subdirectory(cli)
  if(${BUILD_TESTS})
    add_subdirectory(tests)
  endif()
endif()
