cmake_minimum_required(VERSION 3.28)
project(PhigrosAutoPlaySystem VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(PAS_ASAN_SUPPORT_ROOT)
  if(NOT EXISTS "${PAS_ASAN_SUPPORT_ROOT}/sanitizer/common_interface_defs.h")
    message(FATAL_ERROR "PAS_ASAN_SUPPORT_ROOT lacks sanitizer headers")
  endif()
  file(COPY "${PAS_ASAN_SUPPORT_ROOT}/sanitizer" DESTINATION "${CMAKE_BINARY_DIR}/asan_support")
  file(GLOB PAS_ASAN_RUNTIME_LIBS "${PAS_ASAN_SUPPORT_ROOT}/../lib/x64/*asan*.lib")
  file(COPY ${PAS_ASAN_RUNTIME_LIBS} DESTINATION "${CMAKE_BINARY_DIR}/asan_support/lib")
  file(GLOB PAS_ASAN_RUNTIME_DLLS "${PAS_ASAN_SUPPORT_ROOT}/../bin/Hostx64/x64/*asan*.dll")
  file(COPY ${PAS_ASAN_RUNTIME_DLLS} DESTINATION "${CMAKE_BINARY_DIR}/Debug")
  include_directories("${CMAKE_BINARY_DIR}/asan_support")
endif()

find_package(CLI11 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/proto/emulator_controller.proto)
set(PROTO_OUT ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_OUT})
add_custom_command(
  OUTPUT ${PROTO_OUT}/emulator_controller.pb.cc ${PROTO_OUT}/emulator_controller.pb.h
         ${PROTO_OUT}/emulator_controller.grpc.pb.cc ${PROTO_OUT}/emulator_controller.grpc.pb.h
  COMMAND $<TARGET_FILE:protobuf::protoc>
          --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
          --cpp_out=${PROTO_OUT}
          --grpc_out=${PROTO_OUT}
          --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
          ${PROTO_FILE}
  DEPENDS ${PROTO_FILE}
  VERBATIM)
add_library(pas_proto STATIC ${PROTO_OUT}/emulator_controller.pb.cc
                             ${PROTO_OUT}/emulator_controller.grpc.pb.cc)
target_include_directories(pas_proto PUBLIC ${PROTO_OUT})
target_link_libraries(pas_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)

add_library(pas_core STATIC src/core.cpp src/config.cpp src/analysis.cpp src/adb.cpp src/journal.cpp src/touch_evidence.cpp src/preview.cpp)
target_include_directories(pas_core PUBLIC include)
target_link_libraries(pas_core PUBLIC nlohmann_json::nlohmann_json bcrypt windowscodecs ole32 d3d11 dxgi d3dcompiler user32)
if(MSVC)
  target_compile_options(pas_core PRIVATE /W4 /permissive- /EHsc)
endif()

add_library(pas_emulator STATIC src/emulator.cpp)
target_include_directories(pas_emulator PUBLIC include)
target_link_libraries(pas_emulator PUBLIC pas_core pas_proto)
if(MSVC)
  target_compile_options(pas_emulator PRIVATE /W4 /permissive- /EHsc)
endif()

add_library(pas_bench STATIC src/bench.cpp src/bench_workloads.cpp src/touch_bench.cpp src/native_capture.cpp src/scrcpy_capture.cpp)
target_include_directories(pas_bench PUBLIC include)
target_include_directories(pas_bench PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(pas_bench PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(pas_bench PUBLIC pas_emulator pas_core PRIVATE psapi pdh windowsapp runtimeobject ws2_32 dwmapi ${FFMPEG_LIBRARIES})
if(MSVC)
  target_compile_options(pas_bench PRIVATE /W4 /permissive- /EHsc)
endif()

add_executable(pas apps/pas/main.cpp)
target_link_libraries(pas PRIVATE pas_core pas_emulator pas_bench CLI11::CLI11)
if(MSVC)
  target_compile_options(pas PRIVATE /W4 /permissive- /EHsc)
endif()

enable_testing()
add_executable(pas_tests tests/core_tests.cpp tests/grpc_loopback_tests.cpp
                         tests/config_analysis_tests.cpp tests/cli_tests.cpp
                         tests/capture_backend_tests.cpp)
target_link_libraries(pas_tests PRIVATE pas_bench pas_emulator pas_core GTest::gtest_main)
add_dependencies(pas_tests pas)
if(MSVC)
  target_compile_options(pas_tests PRIVATE /W4 /permissive- /EHsc)
endif()
include(GoogleTest)
gtest_discover_tests(pas_tests)
