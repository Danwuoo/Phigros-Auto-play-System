# Optional Windows backend. Requires the retained overlay, NOT stock gRPC.
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC 1.81.1 EXACT CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
set(PAS_PROTO_FILE "${PROJECT_SOURCE_DIR}/proto/emulator_controller.proto")
set(PAS_PROTO_OUT "${PROJECT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${PAS_PROTO_OUT}")
set(PAS_PROTO_PATHS "--proto_path=${PROJECT_SOURCE_DIR}/proto")
get_target_property(PAS_PROTOBUF_INCLUDES protobuf::libprotobuf INTERFACE_INCLUDE_DIRECTORIES)
foreach(PAS_INCLUDE IN LISTS PAS_PROTOBUF_INCLUDES)
  if(IS_DIRECTORY "${PAS_INCLUDE}")
    list(APPEND PAS_PROTO_PATHS "--proto_path=${PAS_INCLUDE}")
  endif()
endforeach()
add_custom_command(
  OUTPUT "${PAS_PROTO_OUT}/emulator_controller.pb.cc" "${PAS_PROTO_OUT}/emulator_controller.pb.h"
         "${PAS_PROTO_OUT}/emulator_controller.grpc.pb.cc" "${PAS_PROTO_OUT}/emulator_controller.grpc.pb.h"
  COMMAND $<TARGET_FILE:protobuf::protoc> ${PAS_PROTO_PATHS}
          "--cpp_out=${PAS_PROTO_OUT}" "--grpc_out=${PAS_PROTO_OUT}"
          --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin> "${PAS_PROTO_FILE}"
  DEPENDS "${PAS_PROTO_FILE}"
  COMMAND_EXPAND_LISTS VERBATIM)
add_library(pas_proto STATIC "${PAS_PROTO_OUT}/emulator_controller.pb.cc"
                            "${PAS_PROTO_OUT}/emulator_controller.grpc.pb.cc")
target_include_directories(pas_proto PUBLIC "${PAS_PROTO_OUT}")
target_link_libraries(pas_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
add_library(pas_emulator STATIC src/emulator.cpp src/grpc_transport.cpp)
target_include_directories(pas_emulator PUBLIC include)
target_link_libraries(pas_emulator PUBLIC pas_core pas_proto nlohmann_json::nlohmann_json)
