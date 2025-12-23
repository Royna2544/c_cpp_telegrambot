find_package(LibXml2 REQUIRED)
find_package(libgit2 REQUIRED)

# ################ Android ROM builder with C++ #################
tgbot_library(
  NAME
  AndroidBuilder-configparse
  SRCS
  ConfigParsers.cpp
  Prepare.cpp
  LIBS
  nlohmann_json::nlohmann_json
  LibXml2::LibXml2
  BuilderUtilsLinux
  NO_LIBPREFIX
  STATIC)

command_module(
  NAME
  rombuild
  SRCS
  module/romBuild.cpp
  tasks/ROMBuildTask.cpp
  tasks/UploadFileTask.cpp
  tasks/RepoSyncTask.cpp
  LIBS
  AndroidBuilder-configparse
  Archives
  Utils
  CurlDownload
  BuilderUtilsLinux
  TgBot
  AuthApi
  PLATFORM
  "Linux")

tgbot_exe(
  NAME
  ConfigParserTest
  SRCS
  ConfigParserTest.cpp
  LIBS
  AndroidBuilder-configparse
  BuilderUtilsLinux
  OPTIONAL)

install(DIRECTORY configs/ DESTINATION share/${PROJECT_NAME}/builder/android)
file(COPY configs/
     DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME}/builder/android)
