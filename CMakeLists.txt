cmake_minimum_required(VERSION 3.10.2)
project(TgBot++)

option(ENFORCE_EN_US "Enforce en-US locale for output" ON)
option(ENABLE_SOCKET_CONNECT "Enable doing various operations with bot" ON)
if(NOT WIN32)
  option(ENABLE_RUNTIME_COMMAND "Enable runtime loader of commands" ON)
endif()
try_compile(IS_NOT_X86 ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/tests/testX86.c)
if (NOT IS_NOT_X86)
  message(INFO " X86 target detected")
  add_compile_options(-mrdrnd)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Werror -O3)
add_compile_options(
  -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)
include_directories(src/include/third-party)
include_directories(src/include)
include_directories(src/)

# Include boost by default
find_package(Boost COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIR})

if(WIN32)
  set(TARGET_VARIANT windows)
else()
  set(TARGET_VARIANT posix)
endif()

set(SRC_LIST
    src/Authorization.cpp
    src/CompilerInTelegram.cpp
    src/BotAddCommand.cpp
    src/ExtArgs.cpp
    src/signalhandler/SignalHandler_${TARGET_VARIANT}.cpp
    src/random/RandomNumberGenerator.cpp
    src/SingleThreadCtrlManager.cpp
    src/SpamBlocker.cpp
    src/RegEXHandler.cpp
    src/TimerImpl.cpp)

# All commands reside here now
include(src/command_modules/CMakeLists.txt)

# Database
find_package(Protobuf REQUIRED)
include(cmake/ProtobufCpp.cmake)
my_protobuf_generate_cpp(proto/gen PROTO_SRCS PROTO_HDRS proto/TgBotDB.proto)
add_library(TgBotDB STATIC src/Database.cpp ${PROTO_SRCS})

get_filename_component(PROTO_HDRS_DIR ${PROTO_HDRS} DIRECTORY)
target_include_directories(TgBotDB PUBLIC ${PROTO_HDRS_DIR})
target_include_directories(TgBotDB PRIVATE ${Protobuf_INCLUDE_DIRS})

# Protobuf dep - ABSL
foreach(ABSL_LIB absl_log_internal_check_op absl_log_internal_message)
  find_library(TMP_${ABSL_LIB} NAMES ${ABSL_LIB})
  if(NOT TMP_${ABSL_LIB})
    message(INFO " Library ${ABSL_LIB} not existent")
    continue()
  endif()
  set(ABSL_LIBS ${ABSL_LIBS} ${ABSL_LIB})
endforeach()

target_link_libraries(TgBotDB TgBot TgBotUtils protobuf::libprotobuf protobuf ${ABSL_LIBS})

if(ENFORCE_EN_US)
  add_definitions(-DLOCALE_EN_US)
endif()

if(ENABLE_RUNTIME_COMMAND)
  add_definitions(-DRTCOMMAND_LOADER)
  set(SRC_LIST src/RTCommandLoader.cpp ${SRC_LIST})
endif()

if(ENABLE_SOCKET_CONNECT)
  set(SOCKET_CLI_NAME ${PROJECT_NAME}_SocketCli)
  add_definitions(-DSOCKET_CONNECTION)
  add_library(TgBotSocket STATIC src/socket/SocketUtils_${TARGET_VARIANT}.cpp
                                 src/socket/TgBotCommandMap.cpp)
  set(SRC_LIST src/SocketConnectionHandler.cpp src/ChatObserver.cpp ${SRC_LIST})
  add_executable(${SOCKET_CLI_NAME} src/socket/TgBotSocketClient.cpp)
  target_link_libraries(${SOCKET_CLI_NAME} TgBotSocket)
  if (WIN32)
    target_link_libraries(${SOCKET_CLI_NAME} Ws2_32)
  endif()
endif()

# TgBot lib + Boost warning
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

# Res library
configure_file(src/ResourceIncBin.cpp.in src/ResourceIncBin.cpp)
add_library(TgBotRes STATIC ${CMAKE_BINARY_DIR}/src/ResourceIncBin.cpp)

# Utils library
add_library(
  TgBotUtils STATIC
  src/ConfigManager.cpp
  src/popen_wdt/popen_wdt_${TARGET_VARIANT}.c
  src/popen_wdt/popen_wdt_lib.cpp
  src/fslib/FileSystemLib.cpp
  src/fslib/FileSystemLib_${TARGET_VARIANT}.cpp)
# Windows needs this
if(WIN32)
  target_link_libraries(TgBotUtils shlwapi)
endif()
target_link_libraries(TgBotUtils TgBotRes ${Boost_LIBRARIES})

# TgBot-Cpp library
add_subdirectory(lib)
if(ENABLE_RUNTIME_COMMAND)
  # Dynamic cmd loader
  add_subdirectory(src/cmd_dynamic)
endif()

# Build libTgBot++
add_library(${PROJECT_NAME} ${SRC_LIST})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/${COMMANDS_CWD})
# Default LD-list
set(LD_LIST TgBot TgBotDB TgBotUtils TgBotRes ${ABSL_LIBS})
# Socket
if(ENABLE_SOCKET_CONNECT)
  set(LD_LIST ${LD_LIST} TgBotSocket)
endif()
# Windows API
if(WIN32)
  set(LD_LIST ${LD_LIST} wsock32 Ws2_32)
endif()
target_link_libraries(${PROJECT_NAME} ${LD_LIST})

# Main launcher
set(PROJECT_MAIN_EXE ${PROJECT_NAME}_main)
add_executable(${PROJECT_MAIN_EXE} src/main.cpp)
target_link_libraries(${PROJECT_MAIN_EXE} ${PROJECT_NAME})
target_include_directories(${PROJECT_MAIN_EXE} PRIVATE ${CMAKE_BINARY_DIR}/${COMMANDS_CWD})

# Protobuf DB Dumper
set(DBDUMPER_NAME ${PROJECT_NAME}_dumpDB)
add_executable(${DBDUMPER_NAME} proto/DumpProtoDB.cc)
target_link_libraries(${DBDUMPER_NAME} TgBotDB TgBotUtils ${ABSL_LIBS})

# Test begins
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
)
FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
    FetchContent_Populate(googletest)
    add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
    include_directories(${gtest_SOURCE_DIR}/include ${gmock_SOURCE_DIR}/include)
endif()