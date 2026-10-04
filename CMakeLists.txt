cmake_minimum_required(VERSION 3.21)
project(AnimatorRF VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ARF_BUILD_APP "Build the Qt app" ON)
if(ANDROID)
  set(_arf_tests_default OFF)
else()
  set(_arf_tests_default ON)
endif()
option(ARF_BUILD_TESTS "Build core tests" ${_arf_tests_default})

if(ARF_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(core)
if(ARF_BUILD_APP)
  add_subdirectory(app)
endif()
