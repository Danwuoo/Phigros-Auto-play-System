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
find_package(gRPC 1.81.1 EXACT CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(FFMPEG REQUIRED)
find_package(ZLIB REQUIRED)

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

add_library(pas_core STATIC src/core.cpp src/config.cpp src/analysis.cpp src/adb.cpp src/journal.cpp src/touch_evidence.cpp src/preview.cpp src/game.cpp src/game_clip_replay.cpp src/game_cold_pipeline.cpp src/game_cold_corpus.cpp src/game_motion.cpp src/game_tracking.cpp src/game_tracking_methods.cpp src/game_tracking_analysis.cpp src/game_tracking_shadow.cpp src/game_dataset.cpp src/game_dataset_tools.cpp src/game_session.cpp src/session_archive.cpp src/session_pixel_clips.cpp src/session_recording.cpp)
target_include_directories(pas_core PUBLIC include)
target_link_libraries(pas_core PUBLIC nlohmann_json::nlohmann_json bcrypt windowscodecs ole32 d3d11 dxgi d3dcompiler user32)
target_link_libraries(pas_core PUBLIC ZLIB::ZLIB)
if(MSVC)
  target_compile_options(pas_core PRIVATE /W4 /permissive- /EHsc)
endif()

# Exact current candidate action closure. Uses the sole existing pas_core;
# this is built into production but typed live admission remains hard closed.
add_library(pas_current_candidate STATIC
  research/bvi_cold_v3/bvi.cpp research/bvi_cold_v3/contact_policy.cpp
  research/bvi_cold_v3/relation_policy.cpp research/bvi_cold_v3/constraint_policy.cpp
  research/hold_front_current/front.cpp research/prelive_current/own_bridge.cpp
  research/bvi_owner_cooperative/extract.cpp
  research/prelive_current/current.cpp)
target_include_directories(pas_current_candidate PUBLIC research/bvi_cold_v3
  research/hold_front_current research/prelive_current)
target_link_libraries(pas_current_candidate PUBLIC pas_core)
if(MSVC)
  target_compile_options(pas_current_candidate PRIVATE /W4 /permissive- /EHsc)
endif()

add_library(pas_emulator STATIC src/emulator.cpp src/grpc_transport.cpp src/runtime.cpp src/manual_session.cpp src/candidate_admission.cpp src/candidate_receipts.cpp)
target_include_directories(pas_emulator PUBLIC include)
target_link_libraries(pas_emulator PUBLIC pas_core pas_proto pas_current_candidate)
execute_process(COMMAND git rev-parse HEAD WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE PAS_BUILD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git status --porcelain WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                OUTPUT_VARIABLE PAS_BUILD_DIRTY OUTPUT_STRIP_TRAILING_WHITESPACE)
if(PAS_BUILD_DIRTY)
  set(PAS_BUILD_DIRTY_JSON true)
else()
  set(PAS_BUILD_DIRTY_JSON false)
endif()
set(PAS_BUILD_HASH_JSON "{")
foreach(PAS_BUILD_FILE src/game.cpp src/game_clip_replay.cpp src/game_cold_pipeline.cpp src/game_cold_corpus.cpp src/game_motion.cpp src/game_tracking.cpp src/game_tracking_methods.cpp
    src/core.cpp src/emulator.cpp src/config.cpp src/analysis.cpp include/pas/analysis.hpp include/pas/core.hpp include/pas/game.hpp include/pas/strategy_version.hpp src/game_session.cpp src/manual_session.cpp src/runtime.cpp src/session_archive.cpp
    src/journal.cpp include/pas/journal.hpp include/pas/result_ui_labels.hpp include/pas/game_session.hpp include/pas/session_archive.hpp include/pas/session_pixel_clips.hpp src/session_pixel_clips.cpp apps/pas/main.cpp include/pas/session_recording.hpp src/session_recording.cpp include/pas/runtime.hpp
    include/pas/candidate_admission.hpp src/candidate_admission.cpp tests/candidate_admission_tests.cpp include/pas/candidate_packet.hpp
    include/pas/candidate_receipts.hpp src/candidate_receipts.cpp tests/candidate_receipt_collector_tests.cpp include/pas/touch_evidence.hpp include/pas/emulator.hpp include/pas/grpc_transport.hpp
    include/pas/config.hpp include/pas/adb.hpp
    research/pre_emulator/qualification_reader/reader/narrow_preflight.hpp
    research/pre_emulator/qualification_reader/reader/component_delta.hpp
    research/pre_emulator/qualification_reader/reader/cooperative_build.hpp
    research/pre_emulator/qualification_reader/reader/cost_reader.hpp
    research/pre_emulator/qualification_reader/reader/graph_reader.hpp
    research/pre_emulator/qualification_reader/reader/streamed_hash.hpp
    research/prelive_current/cost_analysis.cpp research/prelive_current/evidence_hash.hpp
    research/prelive_current/campaign_provenance.hpp research/prelive_current/input_closure.hpp
    research/windows_cost_comparison/metrics.hpp research/windows_qualification/verifier.hpp
    include/pas/diagnostic_snapshot.hpp include/pas/diagnostic_json.hpp include/pas/lossless_event_stream.hpp
    research/prelive_current/owner_candidate_step.hpp research/prelive_current/receipt_stream.hpp
    research/prelive_current/current.hpp research/prelive_current/current.cpp research/prelive_current/tail_current.hpp
    research/prelive_current/own_bridge.hpp research/prelive_current/own_bridge.cpp
    research/prelive_current/owner_checkpoint.hpp research/bvi_owner_cooperative/extract.hpp research/bvi_owner_cooperative/extract.cpp
    research/hold_front_current/front.hpp research/hold_front_current/front.cpp
    research/bvi_cold_v3/bvi.hpp research/bvi_cold_v3/bvi.cpp research/bvi_cold_v3/contact_policy.hpp research/bvi_cold_v3/relation_policy.hpp
    research/bvi_cold_v3/contact_policy.cpp research/bvi_cold_v3/relation_policy.cpp research/bvi_cold_v3/constraint_policy.cpp)
  file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${PAS_BUILD_FILE}" PAS_BUILD_HASH)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${PAS_BUILD_FILE}")
  string(APPEND PAS_BUILD_HASH_JSON "\"${PAS_BUILD_FILE}\":\"${PAS_BUILD_HASH}\",")
endforeach()
string(REGEX REPLACE ",$" "}" PAS_BUILD_HASH_JSON "${PAS_BUILD_HASH_JSON}")
configure_file(cmake/session_build_provenance.hpp.in generated/session_build_provenance.hpp @ONLY)
target_include_directories(pas_emulator PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated" "${CMAKE_CURRENT_SOURCE_DIR}")
if(PAS_HD9_TRACE_BASELINE_LIBRARY)
  add_executable(pas_hd9_original_trace tests/hd9_trace_main.cpp src/game_session.cpp)
  target_include_directories(pas_hd9_original_trace PRIVATE include)
  target_link_libraries(pas_hd9_original_trace PRIVATE "${PAS_HD9_TRACE_BASELINE_LIBRARY}"
      nlohmann_json::nlohmann_json bcrypt windowscodecs ole32 d3d11 dxgi d3dcompiler user32)
endif()
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

# Offline only. The A36 target uses its original headers as well as its frozen
# library, since GameObserver's layout differs from the candidate.
add_executable(pas_pixel_audit apps/pixel_audit/main.cpp)
target_link_libraries(pas_pixel_audit PRIVATE pas_core)
add_executable(pas_miss_review apps/miss_review/main.cpp)
target_link_libraries(pas_miss_review PRIVATE pas_core)
add_executable(pas_recording_check apps/recording_check/main.cpp apps/recording_check/select.cpp)
target_link_libraries(pas_recording_check PRIVATE pas_core mfplat mfreadwrite mfuuid)
if(MSVC)
  target_compile_options(pas_miss_review PRIVATE /W4 /permissive- /EHsc)
endif()
if(PAS_A36_AUDIT_LIBRARY AND PAS_A36_AUDIT_INCLUDE_DIR)
  add_executable(pas_a36_pixel_audit apps/pixel_audit/main.cpp)
  target_include_directories(pas_a36_pixel_audit PRIVATE "${PAS_A36_AUDIT_INCLUDE_DIR}")
  target_link_libraries(pas_a36_pixel_audit PRIVATE "${PAS_A36_AUDIT_LIBRARY}"
      nlohmann_json::nlohmann_json bcrypt windowscodecs ole32 d3d11 dxgi d3dcompiler user32)
endif()
if(MSVC)
  target_compile_options(pas PRIVATE /W4 /permissive- /EHsc)
endif()

enable_testing()
add_executable(pas_disabled_live_seam_tests research/prelive_current/disabled_live_seam_tests.cpp)
target_link_libraries(pas_disabled_live_seam_tests PRIVATE pas_current_candidate)
add_executable(candidate_admission_controls tests/candidate_admission_tests.cpp)
target_link_libraries(candidate_admission_controls PRIVATE pas_emulator)
add_executable(candidate_receipt_collector_controls tests/candidate_receipt_collector_tests.cpp)
target_link_libraries(candidate_receipt_collector_controls PRIVATE pas_emulator)
add_executable(pas_grpc_latency apps/grpc_latency/main.cpp)
target_link_libraries(pas_grpc_latency PRIVATE pas_emulator pas_core CLI11::CLI11 winmm)
if(MSVC)
  target_compile_options(pas_grpc_latency PRIVATE /W4 /permissive- /EHsc)
endif()
if(PAS_GRPC_RESEARCH_SOURCE_DIR)
  message(FATAL_ERROR "Legacy endpoint override is incompatible with the patched dependency; clear PAS_GRPC_RESEARCH_SOURCE_DIR and use --read-chunk-kib")
endif()

add_executable(pas_frame_review apps/frame_review/main.cpp)
target_link_libraries(pas_frame_review PRIVATE pas_core)

# Optional and offline only: CPU LibTorch does not become a PAS dependency.
option(PAS_ENABLE_CPU_VISION "Build offline CPU learned vision experiment" OFF)
if(PAS_ENABLE_CPU_VISION)
  find_package(Torch 2.7.0 EXACT CONFIG REQUIRED)
  file(READ "${TORCH_INSTALL_PREFIX}/build-version" PAS_CPU_TORCH_BUILD)
  string(STRIP "${PAS_CPU_TORCH_BUILD}" PAS_CPU_TORCH_BUILD)
  if(NOT PAS_CPU_TORCH_BUILD STREQUAL "2.7.0+cpu")
    message(FATAL_ERROR "Offline pilot requires the pinned 2.7.0+cpu LibTorch distribution")
  endif()
  include(GoogleTest)
  add_library(pas_vision_cpu_lib STATIC src/vision_cpu.cpp)
  target_include_directories(pas_vision_cpu_lib PUBLIC include)
  target_link_libraries(pas_vision_cpu_lib PUBLIC pas_core PRIVATE "${TORCH_LIBRARIES}")
  separate_arguments(PAS_TORCH_FLAGS NATIVE_COMMAND "${TORCH_CXX_FLAGS}")
  target_compile_options(pas_vision_cpu_lib PRIVATE ${PAS_TORCH_FLAGS})
  add_executable(pas_vision_cpu apps/vision_cpu/main.cpp)
  target_link_libraries(pas_vision_cpu PRIVATE pas_vision_cpu_lib)
  add_executable(pas_vision_cpu_tests tests/vision_cpu_tests.cpp)
  target_link_libraries(pas_vision_cpu_tests PRIVATE pas_vision_cpu_lib GTest::gtest_main)
  target_compile_definitions(pas_vision_cpu_tests PRIVATE PAS_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  if(MSVC)
    target_compile_options(pas_vision_cpu_lib PRIVATE /bigobj /EHsc)
    file(GLOB PAS_CPU_TORCH_DLLS "${TORCH_INSTALL_PREFIX}/lib/*.dll")
    foreach(PAS_CPU_TARGET pas_vision_cpu pas_vision_cpu_tests)
      add_custom_command(TARGET ${PAS_CPU_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PAS_CPU_TORCH_DLLS}
          $<TARGET_FILE_DIR:${PAS_CPU_TARGET}>)
    endforeach()
  endif()
  gtest_discover_tests(pas_vision_cpu_tests PROPERTIES TIMEOUT 30)
endif()

add_executable(pas_tests tests/core_tests.cpp tests/grpc_loopback_tests.cpp
                         tests/grpc_transport_tests.cpp
                         tests/config_analysis_tests.cpp tests/cli_tests.cpp
                         tests/capture_backend_tests.cpp tests/game_tests.cpp tests/game_tracking_tests.cpp tests/game_dataset_tests.cpp tests/cold_scenario_tests.cpp)
target_sources(pas_tests PRIVATE tests/game_session_tests.cpp)
target_sources(pas_tests PRIVATE tests/session_recording_tests.cpp)
target_compile_definitions(pas_tests PRIVATE PAS_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(pas_tests PRIVATE pas_bench pas_emulator pas_core GTest::gtest_main)
add_dependencies(pas_tests pas)
if(MSVC)
  target_compile_options(pas_tests PRIVATE /W4 /permissive- /EHsc)
endif()
include(GoogleTest)
gtest_discover_tests(pas_tests)
