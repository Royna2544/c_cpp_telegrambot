cmake_minimum_required(VERSION 3.10.2)
project(TgBot++ LANGUAGES CXX C)

################ Define a CMake Test Style macro ################
macro(perform_test msg)
  message(STATUS "Performing test ${msg}")
  set(PERFORM_TEST_SUCCESS "Performing test ${msg}: Success")
  set(PERFORM_TEST_FAIL "Performing test ${msg}: Failed")
endmacro()
macro(perform_test_ret result)
  if (${result})
    message(STATUS ${PERFORM_TEST_SUCCESS})
  else()
    message(STATUS ${PERFORM_TEST_FAIL})
  endif()
endmacro()
########### Define a target_link_libraries helper macro ###########
macro(add_lib_if var)
  if(${var})
    set(LD_LIST ${LD_LIST} ${ARGN})
  endif()
endmacro()
macro(target_link_lib_if_windows target)
  if (WIN32)
    target_link_libraries(${target} ${ARGN})
  endif()
endmacro()
#####################################################################

################ EXE NAMES STEMMED FROM PROJECT_NAME ################
set(SOCKET_CLI_NAME ${PROJECT_NAME}_SocketCli)
set(MEDIA_CLI_NAME ${PROJECT_NAME}_MediaCli)
set(DBDUMPER_NAME ${PROJECT_NAME}_dumpDB)
set(PROJECT_MAINEXE_NAME ${PROJECT_NAME}_main)
set(PROJECT_TEST_NAME ${PROJECT_NAME}_test)
#####################################################################

############################## OPTIONS ##############################
option(ENFORCE_EN_US "Enforce en-US locale for output" ON)
option(ENABLE_RUNTIME_COMMAND "Enable runtime loader of commands" ON)
#################### This must be explicitly set ###################
set(BUILD_SHARED_LIBS OFF)
#####################################################################

################# Detecting x86/x86_64 Architecture ################
if(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86_64|AMD64|amd64")
  add_compile_options(-mrdrnd)
endif()
#####################################################################

######################### C++ Configuration #########################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#####################################################################

####################### Global compiler flags #######################
add_compile_options(-Wall -Werror -O3)
add_compile_options(
  -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)
################### Check Windows's AF_UNIX support ###################
################# Versions later than Win10 RS3 does #################
if (WIN32)
  perform_test("USE_UNIX_SOCKETS")
  try_compile(USE_UNIX_SOCKETS ${CMAKE_BINARY_DIR} ${CMAKE_SOURCE_DIR}/tests/test_af_unix.c)
  perform_test_ret(USE_UNIX_SOCKETS)
else()
  set(USE_UNIX_SOCKETS TRUE)
endif()
if (USE_UNIX_SOCKETS)
  add_definitions(-DSOCKET_CONNECTION)
endif()
#####################################################################

################ TgBot lib + Boost creates a warning ################
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
if(ENFORCE_EN_US)
  add_definitions(-DLOCALE_EN_US)
endif()
#####################################################################

######################## Include directories ########################
include_directories(src/include)
include_directories(src/)
#####################################################################

#################### Include directories (boost) ####################
find_package(Boost COMPONENTS program_options REQUIRED)
include_directories(${Boost_INCLUDE_DIR})
#####################################################################

############################# Platforms #############################
if(WIN32)
  set(TARGET_VARIANT windows)
else()
  set(TARGET_VARIANT posix)
endif()
#####################################################################

############################ Source list ############################
set(SRC_LIST
    src/Authorization.cpp
    src/BotAddCommand.cpp
    src/CompilerInTelegram.cpp
    src/ExtArgs.cpp
    src/signalhandler/SignalHandler_${TARGET_VARIANT}.cpp
    src/random/RandomNumberGenerator.cpp
    src/SpamBlocker.cpp
    src/RegEXHandler.cpp
    src/TimerImpl.cpp
)
#####################################################################

######### Commands modules (The commands handler of the bot) #########
################ Commands module in a single file list ################
set(CMD_MODULES_SINGLE_LIST alive flash possibility decide delay decho randsticker fileid)
if (NOT WIN32)
  set(CMD_MODULES_SINGLE_LIST ${CMD_MODULES_SINGLE_LIST} ibash)
endif()
######################## Commands sources list ########################
set(CMD_MODULES_SOURCES_LIST database_impl bash_impl ${CMD_MODULES_SINGLE_LIST} timer_impl)
######################## Commands modules list ########################
set(CMD_MODULES_LIST addblacklist rmblacklist addwhitelist 
      rmwhitelist bash unsafebash ${CMD_MODULES_SINGLE_LIST}
      starttimer stoptimer saveid start)
####################### Commands modules defines #######################
set(COMMANDS_CWD src/command_modules)
set(COMMANDS_GEN_HEADER ${CMAKE_BINARY_DIR}/${COMMANDS_CWD}/cmds.gen.h)
############ Generate commands modules header in compile time ############
FILE(REMOVE ${COMMANDS_GEN_HEADER})
FILE(APPEND ${COMMANDS_GEN_HEADER} "#include <vector>\n")
FILE(APPEND ${COMMANDS_GEN_HEADER} "#include <command_modules/CommandModule.h>\n\n")
foreach(lib ${CMD_MODULES_LIST})
  FILE(APPEND ${COMMANDS_GEN_HEADER} "extern const struct CommandModule cmd_${lib};\n")
endforeach()
foreach(lib ${CMD_MODULES_SOURCES_LIST})
  set(SRC_LIST ${SRC_LIST} ${COMMANDS_CWD}/${lib}.cpp)
endforeach()
FILE(APPEND ${COMMANDS_GEN_HEADER} "\n")
FILE(APPEND ${COMMANDS_GEN_HEADER} "inline const std::vector gCmdModules = {\n")
foreach(lib ${CMD_MODULES_LIST})
  FILE(APPEND ${COMMANDS_GEN_HEADER} "   &cmd_${lib},\n")
endforeach()
FILE(APPEND ${COMMANDS_GEN_HEADER} "};\n")
#####################################################################

####################### Database (Protobuf) #######################
find_package(Protobuf REQUIRED)
include(cmake/ProtobufCpp.cmake)
my_protobuf_generate_cpp(proto/gen PROTO_SRCS PROTO_HDRS proto/TgBotDB.proto)
add_library(TgBotDB STATIC src/Database.cpp
                           src/DatabaseWrapper.cpp
                           ${PROTO_SRCS})
####################### TgBotDB lib includes #######################
get_filename_component(PROTO_HDRS_DIR ${PROTO_HDRS} DIRECTORY)
target_include_directories(TgBotDB PUBLIC ${PROTO_HDRS_DIR})
target_include_directories(TgBotDB PRIVATE ${Protobuf_INCLUDE_DIRS})
########## TgBotDB ABSL libs (We need them on some platforms) ##########
foreach(ABSL_LIB absl_log_internal_check_op absl_log_internal_message)
  perform_test("HAS_LIB_${ABSL_LIB}")
  find_library(TMP_${ABSL_LIB} NAMES ${ABSL_LIB})
  perform_test_ret(TMP_${ABSL_LIB})
  if(NOT TMP_${ABSL_LIB})
    continue()
  endif()
  set(ABSL_LIBS ${ABSL_LIBS} ${ABSL_LIB})
endforeach()
####################### TgBotDB lib includes #######################
target_link_libraries(TgBotDB TgBot TgBotUtils protobuf::libprotobuf protobuf ${ABSL_LIBS})
#####################################################################

################## RTCL (Run Time Command Loader) ##################
if(ENABLE_RUNTIME_COMMAND)
  add_definitions(-DRTCOMMAND_LOADER)
  set(SRC_LIST ${SRC_LIST} src/RTCommandLoader.cpp)
  add_subdirectory(src/cmd_dynamic)
  if (WIN32)
    add_subdirectory(src/third-party/dlfcn-win32)
    set(CMAKE_DL_LIBS dl)
  endif()
endif()
#####################################################################

####################### TgBot Socket Library #######################
if (USE_UNIX_SOCKETS)
  set(SRC_LIST ${SRC_LIST}
      src/ChatObserver.cpp
      src/SocketConnectionHandler.cpp)
  if (UNIX)
    set(SOCKET_SRC_LIST
      src/socket/SocketInterfaceUnix.cpp
      src/socket/impl/SocketInterfacePosixLocal.cpp
      src/socket/impl/SocketInterfacePosixIPv4.cpp
    )
    if (APPLE)
      set(SOCKET_SRC_LIST ${SOCKET_SRC_LIST}
        src/socket/impl/SocketInterfacePosixIPv4Darwin.cpp
      )
    else()
      set(SOCKET_SRC_LIST ${SOCKET_SRC_LIST}
        src/socket/impl/SocketInterfacePosixIPv4Linux.cpp
      )
    endif()
  elseif(WIN32)
      set(SOCKET_SRC_LIST 
        src/socket/SocketInterfaceWindows.cpp
        src/socket/impl/SocketInterfaceWindowsLocal.cpp
      )
  endif()

  add_library(TgBotSocket STATIC
      ${SOCKET_SRC_LIST}
      src/socket/SocketInterfaceBase.cpp
      src/socket/TgBotCommandMap.cpp)
  add_executable(${SOCKET_CLI_NAME} src/socket/TgBotSocketClient.cpp)

  target_link_libraries(${SOCKET_CLI_NAME} TgBotSocket TgBotUtils)
  target_link_lib_if_windows(${SOCKET_CLI_NAME} Ws2_32)
endif()
#####################################################################

################## TgBot Resource (incbin) Library ##################
configure_file(src/ResourceIncBin.cpp.in src/ResourceIncBin.cpp)
add_library(TgBotRes STATIC ${CMAKE_BINARY_DIR}/src/ResourceIncBin.cpp)
#####################################################################

################# TgBot Utilities (generic) Library #################
add_library(
  TgBotUtils STATIC
  src/ConfigManager.cpp
  src/SingleThreadCtrl.cpp
  src/SingleThreadCtrlManager.cpp
  src/popen_wdt/popen_wdt_${TARGET_VARIANT}.c
  src/popen_wdt/popen_wdt_lib.cpp
  src/fslib/FileSystemLib.cpp
  src/fslib/FileSystemLib_${TARGET_VARIANT}.cpp)

target_link_libraries(TgBotUtils ${Boost_LIBRARIES})
target_link_lib_if_windows(TgBotUtils shlwapi)
#####################################################################

#################### TgBot cpp (the core) Library ###################
add_subdirectory(lib)
#####################################################################

################### The Bot's main functionaility ###################
add_library(TgBotCommandModules STATIC ${SRC_LIST})
target_include_directories(TgBotCommandModules PUBLIC ${COMMANDS_CWD})
######################## Libraries to link ########################
set(LD_LIST TgBot TgBotDB TgBotUtils TgBotRes ${ABSL_LIBS})
add_lib_if(USE_UNIX_SOCKETS TgBotSocket)
add_lib_if(WIN32 wsock32 Ws2_32)
add_lib_if(ENABLE_RUNTIME_COMMAND ${CMAKE_DL_LIBS})
target_link_libraries(TgBotCommandModules ${LD_LIST})
#####################################################################

################# The Bot's main launcher (program) #################
add_executable(${PROJECT_MAINEXE_NAME} src/main.cpp)
target_link_libraries(${PROJECT_MAINEXE_NAME} TgBotCommandModules)
#####################################################################

################# Utility Programs (Dump Database) ##################
add_executable(${DBDUMPER_NAME} proto/DumpProtoDB.cc)
target_link_libraries(${DBDUMPER_NAME} TgBotDB TgBotUtils ${ABSL_LIBS})
#####################################################################

############### Utility Programs (Send Media to chat) ################
if (USE_UNIX_SOCKETS)
  add_executable(${MEDIA_CLI_NAME} proto/SendMediaToChat.cc)
  target_link_libraries(${MEDIA_CLI_NAME} TgBotDB TgBotSocket ${ABSL_LIBS})
  target_link_lib_if_windows(${MEDIA_CLI_NAME} Ws2_32)
endif()
#####################################################################

############### Bot functions' Test Arena (googletest) ##############
add_subdirectory(src/third-party/googletest)
include_directories(${gtest_SOURCE_DIR}/include ${gmock_SOURCE_DIR}/include)
###################### Creates the test target ######################
add_executable(${PROJECT_TEST_NAME}
  tests/AuthorizationTest.cpp
  tests/SingleThreadCtrlTest.cpp
  tests/SpamBlockerTest.cpp
  tests/ExtArgsTest.cpp
  tests/ConfigManagerTest.cpp
  tests/RegexHandlerTest.cpp
  tests/DatabaseLoader.cpp
)
target_link_libraries(${PROJECT_TEST_NAME} gtest gtest_main gmock TgBotCommandModules)
add_test(NAME ${PROJECT_TEST_NAME} COMMAND ${PROJECT_TEST_NAME})
#####################################################################
