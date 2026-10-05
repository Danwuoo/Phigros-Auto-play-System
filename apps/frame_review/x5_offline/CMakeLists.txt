cmake_minimum_required(VERSION 3.28)
project(PasEarlyRoleShadow LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
# Reuse the existing reader/SHA utilities and frozen export without editing
# any X1-X4 source or changing the root runtime build.
add_subdirectory(../offline reused)
add_executable(x5_report ../x5_main.cpp ../x5_report.cpp)
target_link_libraries(x5_report PRIVATE x1_core)
target_compile_options(x5_report PRIVATE /W4 /EHsc /utf-8 /bigobj)
add_executable(x5_scope_report ../x5_scope_report.cpp)
target_link_libraries(x5_scope_report PRIVATE x1_core)
target_compile_options(x5_scope_report PRIVATE /W4 /EHsc /utf-8 /bigobj)
add_executable(x5_tests ${X1_REPO}/tests/x5_rule_tests.cpp ../x5_report.cpp)
target_link_libraries(x5_tests PRIVATE x1_core GTest::gtest_main)
target_compile_options(x5_tests PRIVATE /W4 /EHsc /utf-8 /bigobj)
if(X1_SANITIZER_SUPPORT)
  foreach(target x5_report x5_scope_report x5_tests)
    target_include_directories(${target} PRIVATE ${CMAKE_BINARY_DIR}/asan_support)
    target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:/fsanitize=address>
      $<$<CONFIG:Debug>:/D_DISABLE_VECTOR_ANNOTATION>
      $<$<CONFIG:Debug>:/D_DISABLE_STRING_ANNOTATION>
      $<$<CONFIG:Debug>:/D_DISABLE_OPTIONAL_ANNOTATION>)
    target_link_options(${target} PRIVATE $<$<CONFIG:Debug>:/INCREMENTAL:NO>
      $<$<CONFIG:Debug>:/LIBPATH:${CMAKE_BINARY_DIR}/asan_support/lib>)
  endforeach()
endif()
enable_testing()
add_test(NAME x5_tests COMMAND x5_tests --gtest_brief=1)
