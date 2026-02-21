# ################ Android ROM builder with C++ #################
tgbot_library(
  NAME AndroidBuilder-configparse
  SRCS ConfigParsers.cpp
  ALWAYS_STATIC
)

target_link_libraries(AndroidBuilder-configparse PRIVATE nlohmann_json::nlohmann_json)
include(gRPCGenerate)
grpc_generate(ROMBuild)

tgbot_exe(
  NAME ConfigParserTest
  SRCS ConfigParserTest.cpp
  OPTIONAL)

target_link_libraries(ConfigParserTest PRIVATE AndroidBuilder-configparse)

install(DIRECTORY configs/ DESTINATION share/${PROJECT_NAME}/builder/android)
file(COPY configs/
     DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME}/builder/android)
