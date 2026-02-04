install(DIRECTORY configs/ DESTINATION share/${PROJECT_NAME}/builder/kernel)
file(COPY configs/
     DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME}/builder/kernel)
include(gRPCGenerate)
grpc_generate(LinuxKernelBuild)
