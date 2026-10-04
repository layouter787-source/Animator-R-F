add_library(arf_core STATIC
  src/timeline.cpp
  src/project.cpp
  src/rig.cpp
)
target_include_directories(arf_core PUBLIC include)
target_compile_features(arf_core PUBLIC cxx_std_20)

if(ARF_BUILD_TESTS)
  add_executable(arf_core_tests tests/test_core.cpp)
  target_link_libraries(arf_core_tests PRIVATE arf_core)
  add_test(NAME core COMMAND arf_core_tests)
endif()
