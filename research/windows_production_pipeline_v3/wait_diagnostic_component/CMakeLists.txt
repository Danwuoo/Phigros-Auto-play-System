cmake_minimum_required(VERSION 3.28)
project(PasProductionWaitDiagnosticComponent LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(VCPKG_APPLOCAL_DEPS OFF CACHE BOOL "Bound runtime copies only" FORCE)

foreach(name PAS_WORKSPACE PAS_BASE_BUILD_ROOT PAS_DELTA_SOURCE_COMMIT
        PAS_DELTA_MAIN_SHA256 PAS_DELTA_HEADER_SHA256 PAS_DELTA_TEST_SHA256
        PAS_BASE_MANUAL_SHA256 PAS_BASE_SESSION_PROVENANCE_SHA256
        PAS_BASE_CORE_SHA256 PAS_BASE_CURRENT_SHA256 PAS_BASE_PROTO_SHA256
        PAS_RUNTIME_SOURCE_FILE)
 if(NOT DEFINED ${name} OR "${${name}}" STREQUAL "")
  message(FATAL_ERROR "Required explicit component input: ${name}")
 endif()
endforeach()
set(PAS_BASE_SOURCE_COMMIT "12ec0550e882e3b06751d67fa67c43dda3e9312b")
if(NOT PAS_DELTA_SOURCE_COMMIT MATCHES "^[0-9a-f]+$" OR
   PAS_DELTA_SOURCE_COMMIT STREQUAL PAS_BASE_SOURCE_COMMIT)
 message(FATAL_ERROR "Distinct frozen diagnostic delta source is required")
endif()
string(LENGTH "${PAS_DELTA_SOURCE_COMMIT}" COMMIT_LENGTH)
if(NOT COMMIT_LENGTH EQUAL 40)
 message(FATAL_ERROR "Diagnostic source commit length")
endif()
function(require_source_sha path expected)
 if(NOT EXISTS "${path}")
  message(FATAL_ERROR "Missing exact component input: ${path}")
 endif()
 file(SHA256 "${path}" actual)
 if(NOT actual STREQUAL expected)
  message(FATAL_ERROR "Component input SHA differs: ${path}")
 endif()
endfunction()
set(PIPELINE "${PAS_WORKSPACE}/research/windows_production_pipeline_v3")
require_source_sha("${PIPELINE}/main.cpp" "${PAS_DELTA_MAIN_SHA256}")
require_source_sha("${PIPELINE}/poll_gap_diagnostic.hpp" "${PAS_DELTA_HEADER_SHA256}")
require_source_sha("${PIPELINE}/tests/poll_gap_diagnostic_tests.cpp" "${PAS_DELTA_TEST_SHA256}")
require_source_sha("${PAS_WORKSPACE}/src/manual_session.cpp" "${PAS_BASE_MANUAL_SHA256}")
require_source_sha("${PAS_BASE_BUILD_ROOT}/production/generated/session_build_provenance.hpp" "${PAS_BASE_SESSION_PROVENANCE_SHA256}")
require_source_sha("${PAS_BASE_BUILD_ROOT}/production/pas_core.lib" "${PAS_BASE_CORE_SHA256}")
require_source_sha("${PAS_BASE_BUILD_ROOT}/production/pas_current_candidate.lib" "${PAS_BASE_CURRENT_SHA256}")
require_source_sha("${PAS_BASE_BUILD_ROOT}/production/pas_proto.lib" "${PAS_BASE_PROTO_SHA256}")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC 1.81.1 EXACT CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
foreach(lib pas_current_candidate pas_core pas_proto)
 add_library(${lib} STATIC IMPORTED GLOBAL)
 set_target_properties(${lib} PROPERTIES IMPORTED_LOCATION "${PAS_BASE_BUILD_ROOT}/production/${lib}.lib")
endforeach()

# Literal manual/session provenance stays the exact frozen main55 identity.
# The new generated header names the independently compiled diagnostic delta.
file(WRITE "${CMAKE_BINARY_DIR}/frozen_manual_session.cpp"
 "#include \"${PAS_WORKSPACE}/src/manual_session.cpp\"\n")
file(WRITE "${CMAKE_BINARY_DIR}/wait_diagnostic_component_identity.hpp"
 "#pragma once\ninline constexpr auto pas_wait_diagnostic_source_commit=\"${PAS_DELTA_SOURCE_COMMIT}\";\ninline constexpr bool pas_wait_diagnostic_source_dirty=false;\ninline constexpr auto pas_wait_diagnostic_base_source_commit=\"${PAS_BASE_SOURCE_COMMIT}\";\ninline constexpr auto pas_wait_diagnostic_main_sha256=\"${PAS_DELTA_MAIN_SHA256}\";\ninline constexpr auto pas_wait_diagnostic_header_sha256=\"${PAS_DELTA_HEADER_SHA256}\";\n")
add_executable(production_wait_diagnostic_component
 "${PIPELINE}/main.cpp" "${PAS_WORKSPACE}/src/grpc_transport.cpp")
target_include_directories(production_wait_diagnostic_component PRIVATE
 "${PAS_WORKSPACE}/include" "${PAS_WORKSPACE}/research/prelive_current"
 "${PAS_WORKSPACE}/research/bvi_cold_v3" "${PAS_WORKSPACE}/research/hold_front_current"
 "${PAS_BASE_BUILD_ROOT}/generated" "${PAS_BASE_BUILD_ROOT}/production/generated"
 "${CMAKE_BINARY_DIR}")
target_compile_definitions(production_wait_diagnostic_component PRIVATE
 PAS_WAIT_DIAGNOSTIC_COMPONENT=1 PAS_COLD_MANUAL_SHA="${PAS_BASE_MANUAL_SHA256}")
target_link_libraries(production_wait_diagnostic_component PRIVATE
 pas_current_candidate pas_core pas_proto nlohmann_json::nlohmann_json ZLIB::ZLIB
 protobuf::libprotobuf gRPC::grpc++ bcrypt windowscodecs ole32 d3d11 dxgi d3dcompiler user32 psapi)
add_executable(producer_poll_gap_controls "${PIPELINE}/tests/poll_gap_diagnostic_tests.cpp")
target_include_directories(producer_poll_gap_controls PRIVATE
 "${PIPELINE}" "${PAS_WORKSPACE}/include" "${PAS_WORKSPACE}/research/prelive_current" "${CMAKE_BINARY_DIR}")
target_compile_definitions(producer_poll_gap_controls PRIVATE PAS_WAIT_DIAGNOSTIC_COMPONENT=1)
target_link_libraries(producer_poll_gap_controls PRIVATE nlohmann_json::nlohmann_json)
if(MSVC)
 foreach(target production_wait_diagnostic_component producer_poll_gap_controls)
  target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
 endforeach()
endif()
include("${PAS_RUNTIME_SOURCE_FILE}")
list(LENGTH PAS_COMPONENT_RUNTIME_SOURCES runtime_count)
if(NOT runtime_count EQUAL 18)
 message(FATAL_ERROR "Exact frozen Release runtime18 list is required")
endif()
foreach(runtime IN LISTS PAS_COMPONENT_RUNTIME_SOURCES)
 if(NOT EXISTS "${runtime}" OR NOT runtime MATCHES "[.]dll$")
  message(FATAL_ERROR "Missing frozen runtime input: ${runtime}")
 endif()
 file(COPY "${runtime}" DESTINATION "${CMAKE_BINARY_DIR}")
endforeach()
