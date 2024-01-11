cmake_minimum_required(VERSION 3.10.2)
project(tg_cpphost)

option(ENFORCE_EN_US "Enforce en-US locale for output" ON)
option(ENABLE_SOCKET_CONNECT "Enable doing various operations with bot" ON)
if(NOT WIN32)
  option(ENABLE_RUNTIME_COMMAND "Enable runtime loader of commands" ON)
endif()
try_compile(IS_NOT_X86 ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake/tests/testX86.c)
if (NOT IS_NOT_X86)
  message(INFO " X86 target detected")
  add_compile_options(-mrdrnd)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Werror)

set(PWD_REPLACE //${PROJECT_NAME}:)
add_definitions(-DPWD_REPLACE_STR="${PWD_REPLACE}")
add_compile_options(
  -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=${PWD_REPLACE})

include_directories(src/include/third-party)
include_directories(src/include)
include_directories(src/)

if(WIN32)
  set(TARGET_VARIANT windows)
else()
  set(TARGET_VARIANT posix)
endif()

set(SRC_LIST
    src/main.cpp
    src/Authorization.cpp
    src/CompilerInTelegram.cpp
    src/BotAddCommand.cpp
    src/ExtArgs.cpp
    src/exithandlers/handler.cpp
    src/exithandlers/handler_${TARGET_VARIANT}.cpp
    src/random/RandomNumberGenerator.cpp
    src/SpamBlocker.cpp
    src/RegEXHandler.cpp
    src/TimerImpl.cpp)

# Database
find_package(Protobuf REQUIRED)
include(cmake/ProtobufCpp.cmake)
my_protobuf_generate_cpp(proto/gen PROTO_SRCS PROTO_HDRS proto/TgBotDB.proto)
get_filename_component(PROTO_HDRS_DIR ${PROTO_HDRS} DIRECTORY)
include_directories(${PROTO_HDRS_DIR})
include_directories(${Protobuf_INCLUDE_DIRS})
add_library(TgBotDB STATIC src/Database.cpp ${PROTO_SRCS})
target_link_libraries(TgBotDB TgBot TgBotUtils protobuf::libprotobuf)

if(ENFORCE_EN_US)
  add_definitions(-DLOCALE_EN_US)
endif()

if(ENABLE_RUNTIME_COMMAND)
  add_definitions(-DRTCOMMAND_LOADER)
  set(SRC_LIST src/RTCommandLoader.cpp ${SRC_LIST})
endif()

if(ENABLE_SOCKET_CONNECT)
  add_definitions(-DSOCKET_CONNECTION)
  add_library(TgBotSocket STATIC src/socket/SocketUtils_${TARGET_VARIANT}.cpp
                                 src/socket/TgBotCommandMap.cpp)
  set(SRC_LIST src/SocketConnectionHandler.cpp src/ChatObserver.cpp ${SRC_LIST})
  add_executable(tgbotsocket_cli src/socket/TgBotSocketClient.cpp)
  target_link_libraries(tgbotsocket_cli TgBotSocket)
  if (WIN32)
    target_link_libraries(tgbotsocket_cli Ws2_32)
  endif()
endif()

# TgBot lib + Boost warning
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

# Res library
add_library(TgBotRes STATIC src/ResourceIncBin.cpp)
target_include_directories(TgBotRes PRIVATE .)

# Utils library
add_library(
  TgBotUtils STATIC
  src/popen_wdt/popen_wdt_${TARGET_VARIANT}.c
  src/popen_wdt/popen_wdt_lib.cpp
  src/fslib/FileSystemLib_${TARGET_VARIANT}.cpp)

# Windows needs this
if(WIN32)
  target_link_libraries(TgBotUtils shlwapi)
endif()
target_link_libraries(TgBotUtils TgBotRes)

# TgBot-Cpp library
add_subdirectory(lib)
if(ENABLE_RUNTIME_COMMAND)
  # Dynamic cmd loader
  add_subdirectory(src/cmd_dynamic)
endif()
# Build main tg_cpphost exe
add_executable(${PROJECT_NAME} ${SRC_LIST})

# Default LD-list
set(LD_LIST TgBot TgBotDB TgBotUtils TgBotRes)

# Socket
if(ENABLE_SOCKET_CONNECT)
  set(LD_LIST TgBotSocket ${LD_LIST})
endif()

# Protobuf dep
foreach(ABSL_LIB absl_log_internal_check_op absl_log_internal_message)
  find_library(TMP_${ABSL_LIB} NAMES ${ABSL_LIB})
  if(NOT TMP_${ABSL_LIB})
    message(INFO " Library ${ABSL_LIB} not existent")
    continue()
  endif()
  set(LD_LIST ${LD_LIST} ${TMP_${ABSL_LIB}})
endforeach()

# Windows API
if(WIN32)
  set(LD_LIST ${LD_LIST} wsock32 Ws2_32)
endif()

target_link_libraries(${PROJECT_NAME} ${LD_LIST})

# Protobuf DB Dumper
add_executable(dumpProtoDB proto/DumpProtoDB.cc)
target_link_libraries(dumpProtoDB ${LD_LIST})
