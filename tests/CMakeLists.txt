cmake_minimum_required(VERSION 3.15)
project(elementary_tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(timevalue_tests TimeValue_Tests.cpp)

target_link_libraries(timevalue_tests PRIVATE elem::runtime)

enable_testing()
add_test(NAME timevalue_tests COMMAND timevalue_tests)
