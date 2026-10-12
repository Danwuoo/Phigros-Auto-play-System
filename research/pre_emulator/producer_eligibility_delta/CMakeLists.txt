cmake_minimum_required(VERSION 3.28)
project(PasColdProducerEligibilityDelta LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "Exact bounded runtime copies" FORCE)
option(PAS_MOTION_SOURCE_DELTA "Exact one-member motion source overlay" OFF)
option(PAS_DENSE_SOURCE_DELTA "Exact white-component note-decode source overlay" OFF)
option(PAS_FRAME_METADATA_DELTA "Fixed cold chain metadata reuse" OFF)
foreach(name PAS_SOURCE_WORKSPACE PAS_BASE_PRODUCTION_ROOT PAS_DELTA_SOURCE_COMMIT
        PAS_LIBRARY_INPUTS_FILE PAS_RUNTIME_INPUTS_FILE PAS_BASE_MANUAL_SHA256
        PAS_WORKER_AFFINITY_SHA256 PAS_PRODUCER_PLACEMENT_SHA256
        PAS_BASE_APP_OBJECT PAS_BASE_APP_OBJECT_SHA256
        PAS_CANDIDATE_ADMISSION_SHA256 PAS_CANDIDATE_ADMISSION_TESTS_SHA256)
 if(NOT DEFINED ${name} OR "${${name}}" STREQUAL "")
  message(FATAL_ERROR "Explicit eligibility delta input required: ${name}")
 endif()
endforeach()
string(LENGTH "${PAS_DELTA_SOURCE_COMMIT}" commit_length)
if(NOT commit_length EQUAL 40 OR NOT PAS_DELTA_SOURCE_COMMIT MATCHES "^[0-9a-f]+$")
 message(FATAL_ERROR "Frozen clean delta source identity required")
endif()
function(require_sha path expected)
 if(NOT EXISTS "${path}")
  message(FATAL_ERROR "Missing exact eligibility delta input: ${path}")
 endif()
 file(SHA256 "${path}" actual)
 if(NOT actual STREQUAL expected)
  message(FATAL_ERROR "Eligibility delta input SHA differs: ${path}")
 endif()
endfunction()
set(RAILS "${PAS_SOURCE_WORKSPACE}/research/prelive_current")
set(PIPELINE "${PAS_SOURCE_WORKSPACE}/research/windows_production_pipeline_v3")
require_sha("${RAILS}/worker_affinity.hpp" "${PAS_WORKER_AFFINITY_SHA256}")
require_sha("${PIPELINE}/producer_placement.hpp" "${PAS_PRODUCER_PLACEMENT_SHA256}")
require_sha("${PAS_SOURCE_WORKSPACE}/src/manual_session.cpp" "${PAS_BASE_MANUAL_SHA256}")
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC 1.81.1 EXACT CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(FFMPEG REQUIRED)
include("${PAS_LIBRARY_INPUTS_FILE}")
# These are ten exact roles, not ten new compilations. CPU and production
# cores have different archive bytes and must never share an imported target.
foreach(role cpu_current_rails cpu_current_front cpu_exact_bvi_v3 cpu_own_bridge
             cpu_pas_core production_pas_core production_pas_current_candidate
             production_pas_proto verification_pas_emulator verification_pas_bench)
 if(NOT DEFINED PAS_${role}_PATH OR NOT DEFINED PAS_${role}_SHA256)
  message(FATAL_ERROR "Missing declared imported role: ${role}")
 endif()
 require_sha("${PAS_${role}_PATH}" "${PAS_${role}_SHA256}")
 add_library(${role} STATIC IMPORTED GLOBAL)
 set_target_properties(${role} PROPERTIES IMPORTED_LOCATION "${PAS_${role}_PATH}")
endforeach()
require_sha("${PAS_BASE_APP_OBJECT}" "${PAS_BASE_APP_OBJECT_SHA256}")
require_sha("${PAS_SOURCE_WORKSPACE}/src/candidate_admission.cpp" "${PAS_CANDIDATE_ADMISSION_SHA256}")
require_sha("${PAS_SOURCE_WORKSPACE}/tests/candidate_admission_tests.cpp" "${PAS_CANDIDATE_ADMISSION_TESTS_SHA256}")
file(WRITE "${CMAKE_BINARY_DIR}/frozen_manual_session.cpp"
 "#include \"${PAS_SOURCE_WORKSPACE}/src/manual_session.cpp\"\n")
# The literal manual/session generated identity remains base55. This separate
# identity describes only the newly compiled cold eligibility harness.
file(WRITE "${CMAKE_BINARY_DIR}/producer_eligibility_delta_identity.hpp"
 "#pragma once\ninline constexpr auto pas_producer_eligibility_delta_source_commit=\"${PAS_DELTA_SOURCE_COMMIT}\";\ninline constexpr bool pas_producer_eligibility_delta_source_dirty=false;\ninline constexpr auto pas_producer_eligibility_delta_base_source_commit=\"12ec0550e882e3b06751d67fa67c43dda3e9312b\";\ninline constexpr auto pas_producer_eligibility_delta_worker_affinity_sha256=\"${PAS_WORKER_AFFINITY_SHA256}\";\ninline constexpr auto pas_producer_eligibility_delta_producer_placement_sha256=\"${PAS_PRODUCER_PLACEMENT_SHA256}\";\n")
file(WRITE "${CMAKE_BINARY_DIR}/verification_delta_identity.hpp"
 "#pragma once\ninline constexpr auto pas_cold_verification_delta_source_commit=\"${PAS_DELTA_SOURCE_COMMIT}\";\ninline constexpr bool pas_cold_verification_delta_source_dirty=false;\ninline constexpr auto pas_cold_verification_delta_base_source_commit=\"12ec0550e882e3b06751d67fa67c43dda3e9312b\";\ninline constexpr auto pas_cold_verification_delta_candidate_admission_sha256=\"${PAS_CANDIDATE_ADMISSION_SHA256}\";\ninline constexpr auto pas_cold_verification_delta_candidate_admission_tests_sha256=\"${PAS_CANDIDATE_ADMISSION_TESTS_SHA256}\";\n")
add_executable(current_chain "${RAILS}/chain.cpp")
target_link_libraries(current_chain PRIVATE cpu_current_rails cpu_current_front
 cpu_own_bridge cpu_exact_bvi_v3 cpu_pas_core nlohmann_json::nlohmann_json
 ZLIB::ZLIB bcrypt windowscodecs ole32 d3d11 dxgi d3dcompiler user32 psapi)
add_executable(worker_affinity_controls "${RAILS}/tests/worker_affinity_tests.cpp")
target_link_libraries(worker_affinity_controls PRIVATE nlohmann_json::nlohmann_json)
add_executable(production_pipeline_cold "${PIPELINE}/main.cpp"
 "${PAS_SOURCE_WORKSPACE}/src/grpc_transport.cpp")
add_executable(score_only_controls
 "${PAS_SOURCE_WORKSPACE}/research/pre_emulator/score_only_controls.cpp"
 "${PAS_SOURCE_WORKSPACE}/src/grpc_transport.cpp")
foreach(target production_pipeline_cold score_only_controls)
 target_include_directories(${target} PRIVATE "${PAS_BASE_PRODUCTION_ROOT}/generated"
  "${PAS_BASE_PRODUCTION_ROOT}/production/generated")
 target_compile_definitions(${target} PRIVATE PAS_COLD_MANUAL_SHA="${PAS_BASE_MANUAL_SHA256}")
 target_link_libraries(${target} PRIVATE production_pas_current_candidate
  production_pas_core production_pas_proto nlohmann_json::nlohmann_json
  ZLIB::ZLIB protobuf::libprotobuf gRPC::grpc++ bcrypt windowscodecs ole32
  d3d11 dxgi d3dcompiler user32 psapi)
endforeach()
add_library(candidate_admission_delta OBJECT
 "${PAS_SOURCE_WORKSPACE}/src/candidate_admission.cpp")
target_include_directories(candidate_admission_delta PRIVATE "${PAS_SOURCE_WORKSPACE}"
 "${PAS_SOURCE_WORKSPACE}/include" "${PAS_BASE_PRODUCTION_ROOT}/production/generated"
 "${CMAKE_BINARY_DIR}")
target_compile_definitions(candidate_admission_delta PRIVATE PAS_COLD_VERIFICATION_DELTA=1)
target_link_libraries(candidate_admission_delta PRIVATE nlohmann_json::nlohmann_json)
# Explicit new object precedes old archives. The old candidate_admission member
# is not extracted; all other app/runtime/archive roles retain base55 identity.
set_source_files_properties("${PAS_BASE_APP_OBJECT}" PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
add_executable(pas "${PAS_BASE_APP_OBJECT}" $<TARGET_OBJECTS:candidate_admission_delta>)
set_target_properties(pas PROPERTIES LINKER_LANGUAGE CXX)
target_link_directories(pas PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(pas PRIVATE verification_pas_bench verification_pas_emulator
 production_pas_current_candidate production_pas_core production_pas_proto
 CLI11::CLI11 nlohmann_json::nlohmann_json ZLIB::ZLIB protobuf::libprotobuf
 gRPC::grpc++ bcrypt windowscodecs ole32 d3d11 dxgi d3dcompiler user32 psapi
 pdh windowsapp runtimeobject ws2_32 dwmapi ${FFMPEG_LIBRARIES})
add_executable(candidate_admission_controls
 "${PAS_SOURCE_WORKSPACE}/tests/candidate_admission_tests.cpp"
 $<TARGET_OBJECTS:candidate_admission_delta>)
target_include_directories(candidate_admission_controls PRIVATE "${PAS_SOURCE_WORKSPACE}"
 "${PAS_SOURCE_WORKSPACE}/include" "${CMAKE_BINARY_DIR}")
target_compile_definitions(candidate_admission_controls PRIVATE PAS_COLD_VERIFICATION_DELTA=1)
target_link_libraries(candidate_admission_controls PRIVATE verification_pas_emulator
 production_pas_current_candidate production_pas_core production_pas_proto
 nlohmann_json::nlohmann_json ZLIB::ZLIB protobuf::libprotobuf gRPC::grpc++
 bcrypt windowscodecs ole32 d3d11 dxgi d3dcompiler user32 psapi)
foreach(target current_chain worker_affinity_controls production_pipeline_cold score_only_controls)
 target_include_directories(${target} PRIVATE "${PAS_SOURCE_WORKSPACE}"
  "${PAS_SOURCE_WORKSPACE}/include" "${RAILS}"
  "${PAS_SOURCE_WORKSPACE}/research/bvi_cold_v3"
  "${PAS_SOURCE_WORKSPACE}/research/hold_front_current" "${CMAKE_BINARY_DIR}")
 target_compile_definitions(${target} PRIVATE PAS_PRODUCER_ELIGIBILITY_DELTA=1)
endforeach()
add_executable(qualification_reader
 "${PAS_SOURCE_WORKSPACE}/research/pre_emulator/qualification_reader/main.cpp")
add_executable(qualification_reader_controls
 "${PAS_SOURCE_WORKSPACE}/research/pre_emulator/qualification_reader/controls.cpp")
foreach(target qualification_reader qualification_reader_controls)
 target_include_directories(${target} PRIVATE "${PAS_SOURCE_WORKSPACE}")
 target_link_libraries(${target} PRIVATE nlohmann_json::nlohmann_json bcrypt)
endforeach()
if(PAS_MOTION_SOURCE_DELTA)
 foreach(name PAS_MOTION_SOURCE_SHA256 PAS_MOTION_RUNTIME_CONTRACT_SOURCE
              PAS_MOTION_RUNTIME_CONTRACT_SHA256 PAS_TEST_SOURCE_ROOT)
  if(NOT DEFINED ${name} OR "${${name}}" STREQUAL "")
   message(FATAL_ERROR "Explicit motion overlay input required: ${name}")
  endif()
 endforeach()
 require_sha("${PAS_SOURCE_WORKSPACE}/src/game_motion.cpp" "${PAS_MOTION_SOURCE_SHA256}")
 require_sha("${PAS_MOTION_RUNTIME_CONTRACT_SOURCE}" "${PAS_MOTION_RUNTIME_CONTRACT_SHA256}")
 find_package(GTest CONFIG REQUIRED)
 add_library(game_motion_delta OBJECT "${PAS_SOURCE_WORKSPACE}/src/game_motion.cpp")
 target_include_directories(game_motion_delta PRIVATE "${PAS_SOURCE_WORKSPACE}/include")
 target_link_libraries(game_motion_delta PRIVATE nlohmann_json::nlohmann_json)
 # The shared object is explicit before archive arguments, so neither old
 # core archive extracts its game_motion member. Both archives remain sealed.
 foreach(target current_chain production_pipeline_cold score_only_controls pas candidate_admission_controls)
  target_sources(${target} PRIVATE $<TARGET_OBJECTS:game_motion_delta>)
 endforeach()
 add_executable(motion_source_regression
  "${PAS_SOURCE_WORKSPACE}/tests/game_tests.cpp"
  "${PAS_SOURCE_WORKSPACE}/tests/cold_scenario_tests.cpp"
  "${PAS_MOTION_RUNTIME_CONTRACT_SOURCE}" $<TARGET_OBJECTS:game_motion_delta>)
 target_include_directories(motion_source_regression PRIVATE
  "${PAS_SOURCE_WORKSPACE}/include" "${PAS_BASE_PRODUCTION_ROOT}/generated")
 target_compile_definitions(motion_source_regression PRIVATE PAS_TEST_SOURCE_DIR="${PAS_TEST_SOURCE_ROOT}")
 target_link_libraries(motion_source_regression PRIVATE production_pas_core
  GTest::gtest_main nlohmann_json::nlohmann_json ZLIB::ZLIB protobuf::libprotobuf
  gRPC::grpc++ bcrypt windowscodecs ole32 d3d11 dxgi d3dcompiler user32 psapi)
 if(MSVC)
  target_compile_options(game_motion_delta PRIVATE /W4 /permissive- /EHsc)
  target_compile_options(motion_source_regression PRIVATE /W4 /permissive- /EHsc)
  target_link_options(motion_source_regression PRIVATE /INCREMENTAL:NO /STACK:8388608)
 endif()
endif()
if(PAS_DENSE_SOURCE_DELTA)
 if(NOT PAS_MOTION_SOURCE_DELTA)
  message(FATAL_ERROR "Dense overlay requires the preserved motion overlay")
 endif()
 if(NOT DEFINED PAS_DENSE_SOURCE_SHA256 OR "${PAS_DENSE_SOURCE_SHA256}" STREQUAL "")
  message(FATAL_ERROR "Explicit Dense source SHA required")
 endif()
 require_sha("${PAS_SOURCE_WORKSPACE}/src/game.cpp" "${PAS_DENSE_SOURCE_SHA256}")
 add_library(game_delta OBJECT "${PAS_SOURCE_WORKSPACE}/src/game.cpp")
 target_include_directories(game_delta PRIVATE "${PAS_SOURCE_WORKSPACE}/include")
 target_link_libraries(game_delta PRIVATE nlohmann_json::nlohmann_json)
 foreach(target current_chain production_pipeline_cold score_only_controls
                pas candidate_admission_controls motion_source_regression)
  target_sources(${target} PRIVATE $<TARGET_OBJECTS:game_delta>)
 endforeach()
 if(MSVC)
  target_compile_options(game_delta PRIVATE /W4 /permissive- /EHsc)
 endif()
endif()
if(PAS_FRAME_METADATA_DELTA)
 require_sha("${RAILS}/frame_metadata.hpp" "${PAS_FRAME_METADATA_HEADER_SHA256}")
 require_sha("${RAILS}/tests/frame_metadata_tests.cpp" "${PAS_FRAME_METADATA_TESTS_SHA256}")
 add_executable(frame_metadata_controls "${RAILS}/tests/frame_metadata_tests.cpp")
 target_include_directories(frame_metadata_controls PRIVATE "${PAS_SOURCE_WORKSPACE}/include" "${RAILS}")
 target_compile_definitions(frame_metadata_controls PRIVATE PAS_FRAME_METADATA_DELTA_SOURCE_COMMIT="${PAS_DELTA_SOURCE_COMMIT}" PAS_FRAME_METADATA_DELTA_SOURCE_DIRTY=0)
 target_link_libraries(frame_metadata_controls PRIVATE cpu_pas_core nlohmann_json::nlohmann_json bcrypt)
 if(MSVC)
  target_compile_options(frame_metadata_controls PRIVATE /W4 /permissive- /EHsc)
  target_link_options(frame_metadata_controls PRIVATE /INCREMENTAL:NO /STACK:8388608)
 endif()
endif()
if(MSVC)
 foreach(target current_chain worker_affinity_controls production_pipeline_cold
                score_only_controls qualification_reader qualification_reader_controls
                candidate_admission_controls pas)
  target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
  target_link_options(${target} PRIVATE /INCREMENTAL:NO /STACK:8388608)
 endforeach()
 target_compile_options(candidate_admission_delta PRIVATE /W4 /permissive- /EHsc)
endif()
include("${PAS_RUNTIME_INPUTS_FILE}")
list(LENGTH PAS_ELIGIBILITY_RUNTIME_SOURCES runtime_count)
if(NOT runtime_count EQUAL 18)
 message(FATAL_ERROR "Exact preserved Release runtime18 required")
endif()
foreach(runtime IN LISTS PAS_ELIGIBILITY_RUNTIME_SOURCES)
 if(NOT EXISTS "${runtime}" OR NOT runtime MATCHES "[.]dll$")
  message(FATAL_ERROR "Missing original Release runtime input: ${runtime}")
 endif()
 file(COPY "${runtime}" DESTINATION "${CMAKE_BINARY_DIR}")
endforeach()
