macro(grpc_generate name)

# These packages provide the libraries and the 'protoc' compiler
find_package(gRPC CONFIG REQUIRED)
find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)

# Define paths
set(PROTO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/proto")
set(PROTO_FILE "${PROTO_DIR}/${name}_service.proto")

# Where we want the generated C++ files to land
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Find the gRPC C++ plugin (generates the .grpc.pb.cc files)
get_target_property(GRPC_CPP_PLUGIN gRPC::grpc_cpp_plugin LOCATION)

# This command runs 'protoc' whenever your .proto file changes.
add_custom_command(
    OUTPUT "${GENERATED_DIR}/${name}_service.pb.cc"
           "${GENERATED_DIR}/${name}_service.pb.h"
           "${GENERATED_DIR}/${name}_service.grpc.pb.cc"
           "${GENERATED_DIR}/${name}_service.grpc.pb.h"
    COMMAND protobuf::protoc
    ARGS --grpc_out="${GENERATED_DIR}"
         --cpp_out="${GENERATED_DIR}"
         --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}"
         -I "${PROTO_DIR}"
         "${PROTO_FILE}"
    DEPENDS "${PROTO_FILE}"
    COMMENT "Generating gRPC C++ sources from ${name}_service.proto"
)

add_library(${name}ServiceProto STATIC
    "${GENERATED_DIR}/${name}_service.pb.cc"
    "${GENERATED_DIR}/${name}_service.grpc.pb.cc"
    "${GENERATED_DIR}/${name}_service.pb.h"
    "${GENERATED_DIR}/${name}_service.grpc.pb.h"
)

# Link the generated code to gRPC and Protobuf libraries
target_link_libraries(${name}ServiceProto
    PUBLIC
    protobuf::libprotobuf
    gRPC::grpc++
    absl::base
    absl::strings
)

# Expose the generated header location to anyone who links this lib
target_include_directories(${name}ServiceProto PUBLIC "${GENERATED_DIR}")
endmacro()