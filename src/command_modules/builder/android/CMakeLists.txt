# ################ Android ROM builder with C++ #################
tgbot_library(
  NAME
  AndroidBuilder-configparse
  SRCS
  ConfigParsers.cpp
  LIBS
  nlohmann_json::nlohmann_json
  NO_LIBPREFIX
  STATIC)

include(gRPCGenerate)
grpc_generate(ROMBuild)

command_module(
  NAME
  rombuild
  SRCS
  module/romBuild.cpp
  LIBS
  AndroidBuilder-configparse
  Utils
  TgBot
  AuthApi
  ROMBuildServiceProto
  SystemMonitorServiceProto
  HealthCheckServiceProto)

tgbot_exe(
  NAME
  ConfigParserTest
  SRCS
  ConfigParserTest.cpp
  LIBS
  AndroidBuilder-configparse
  OPTIONAL)

install(DIRECTORY configs/ DESTINATION share/${PROJECT_NAME}/builder/android)
file(COPY configs/
     DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME}/builder/android)
