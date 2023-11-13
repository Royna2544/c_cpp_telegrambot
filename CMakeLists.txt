cmake_minimum_required(VERSION 3.10.2)
project(tg_cpphost)

option(ENFORCE_EN_US "Enforce en-US locale for output" ON)
if(NOT WIN32)
  option(ENABLE_RUNTIME_COMMAND "Enable runtime loader of commands" ON)
  option(ENABLE_SOCKET_CONNECT "Enable doing various operations with bot" ON)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PWD_REPLACE //${PROJECT_NAME}:)
add_definitions(-DPWD_REPLACE_STR="${PWD_REPLACE}")
add_compile_options(
  -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=${PWD_REPLACE})

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
    src/exithandlers/handler_${TARGET_VARIANT}.cpp
    src/SpamBlocker.cpp
    src/TimerImpl.cpp)

set(LIBUTILS_SRC_LIST
    src/utils/libutils.cpp src/popen_wdt/popen_wdt_${TARGET_VARIANT}.c
    src/utils/libutils_${TARGET_VARIANT}.cpp)

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
  set(SRC_LIST src/socket/SocketUtils.cpp src/socket/TgBotCommandStrMap.cpp
               src/SocketConnectionHandler.cpp ${SRC_LIST})
  add_executable(
    tgbotsocket_cli
    src/socket/SocketUtils.cpp src/socket/TgBotCommandStrMap.cpp
    src/socket/TgBotSocketClient.cpp)
endif()

# TgBot lib + Boost warning
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

# Utils library
add_library(TgBotUtils STATIC ${LIBUTILS_SRC_LIST})
# Windows needs this
if(WIN32)
  target_link_libraries(TgBotUtils shlwapi)
endif()

# TgBot-Cpp library
add_subdirectory(lib)
if(ENABLE_RUNTIME_COMMAND)
  # Dynamic cmd loader
  add_subdirectory(src/cmd_dynamic)
endif()
# Build main tg_cpphost exe
add_executable(${PROJECT_NAME} ${SRC_LIST})

# Default LD-list
set(LD_LIST TgBot TgBotUtils)

foreach(ABSL_LIB absl_log_internal_check_op absl_log_internal_message)
  find_library(TMP_${ABSL_LIB} NAMES ${ABSL_LIB})
  if(NOT TMP_${ABSL_LIB})
    message(INFO " Library ${ABSL_LIB} not existent")
    continue()
  endif()
  set(LD_LIST ${LD_LIST} ${TMP_${ABSL_LIB}})
endforeach()
set(LD_LIST ${LD_LIST} TgBotDB)
if(WIN32)
  set(LD_LIST ${LD_LIST} wsock32 Ws2_32)
endif()
target_link_libraries(${PROJECT_NAME} ${LD_LIST})

# Protobuf DB Dumper
add_executable(dumpProtoDB proto/DumpProtoDB.cc)
target_link_libraries(dumpProtoDB ${LD_LIST})
