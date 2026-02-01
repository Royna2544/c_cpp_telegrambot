
install(DIRECTORY configs/ DESTINATION share/${PROJECT_NAME}/builder/kernel)
file(COPY configs/
     DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME}/builder/kernel)
include(gRPCGenerate)
grpc_generate(LinuxKernelBuild)

command_module(
  NAME
  kernelbuild
  SRCS
  kernelbuild.cpp
  ConfigParsers2.cpp
  LIBS
  AuthApi
  nlohmann_json::nlohmann_json
  Utils
  progress
  LinuxKernelBuildServiceProto
)

add_subdirectory(module)
