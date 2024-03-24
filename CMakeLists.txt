cmake_minimum_required(VERSION 3.13)
project(TgBot++ LANGUAGES CXX C)

####################### Set CMake Policy #######################
cmake_policy(SET CMP0077 NEW) # option() command prefers to do nothing
                              # when a normal variable of the given name already exists.
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
########### Define a macro to prevent set(VAR ${VAR} NewVar) ###########
macro(extend_set var)
  set(${var} ${${var}} ${ARGN})
endmacro()
macro(extend_set_if var cond)
  if(${cond})
    extend_set(${var} ${ARGN})
  endif()
endmacro()
########### Define a target_link_libraries helper macro ###########
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
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build libs as shared libraries" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(BUILD_CLI OFF CACHE BOOL "[libgit2] Build client" FORCE)
set(USE_HTTPS "OpenSSL" CACHE STRING "[libgit2] Select HTTPS backend" FORCE)
#####################################################################
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE STRING 
  "Output to store binaries (win32)" FORCE)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin CACHE STRING
  "Output to store binaries (linux)" FORCE)
#####################################################################

################# Detecting x86/x86_64 Architecture ################
if(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86_64|AMD64|amd64" AND NOT MSVC)
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
if (NOT MSVC)
  add_compile_options(
    -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)
else()
  # Windows 10
  add_definitions(-D_WIN32_WINNT=0x0A00)
endif()
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

############################# Check lib #############################
find_package(CURL)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
if (APPLE)
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)
    find_library(SECURITY Security REQUIRED)
    set(MACOS_SSL_LIBS ${CORE_FOUNDATION} ${SECURITY})
endif()
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
    src/command_modules/compiler/Base.cpp
    src/command_modules/compiler/Bash.cpp
    src/command_modules/compiler/CCpp.cpp
    src/command_modules/compiler/Generic.cpp
    src/command_modules/compiler/Helper.cpp
    src/command_modules/CommandModules.cpp
    src/ExtArgs.cpp
    src/libos/libsighandler_impl.cpp
    src/libos/libsighandler_${TARGET_VARIANT}.cpp
    src/random/RandomNumberGenerator.cpp
    src/SpamBlocker.cpp
    src/RegEXHandler.cpp
    src/TimerImpl.cpp
)
#####################################################################

######### Commands modules (The commands handler of the bot) #########
################ Commands module in a single file list ################
set(CMD_MODULES_SINGLE_LIST alive flash possibility decide delay decho
                            randsticker fileid updatecommands rtload)
if (NOT WIN32)
  set(CMD_MODULES_SINGLE_LIST ${CMD_MODULES_SINGLE_LIST} ibash restart)
endif()
######################## Commands sources list ########################
set(CMD_MODULES_SOURCES_LIST database_impl bash_impl ${CMD_MODULES_SINGLE_LIST} timer_impl)
######################## Commands modules list ########################
set(CMD_MODULES_LIST addblacklist rmblacklist addwhitelist 
      rmwhitelist bash ubash ${CMD_MODULES_SINGLE_LIST}
      starttimer stoptimer saveid start)
####################### Commands modules defines #######################
set(COMMANDS_CWD src/command_modules)
set(COMMANDS_GEN_FILE ${COMMANDS_CWD}/gen/cmds.gen.cpp)
########## Generate commands modules list in compile time (cpp) ##########
macro(append_cmdlib lib)
  string(APPEND COMMANDS_MODULES_DECL "extern CommandModule cmd_${lib};\n")
  string(APPEND COMMANDS_MODULES_PTR "&cmd_${lib}")
endmacro()

list(GET CMD_MODULES_LIST 0 COMMANDS_MODULES_FIRST_ELEM)
append_cmdlib(${COMMANDS_MODULES_FIRST_ELEM})
foreach(lib ${CMD_MODULES_LIST})
  if (${lib} STREQUAL ${COMMANDS_MODULES_FIRST_ELEM})
    continue()
  endif()
  string(APPEND COMMANDS_MODULES_PTR ",\n")
  append_cmdlib(${lib})
endforeach()
########## Generate commands modules list in compile time (cpp) ##########
foreach(lib ${CMD_MODULES_SOURCES_LIST})
  extend_set(SRC_LIST ${COMMANDS_CWD}/${lib}.cpp)
endforeach()
#################### Include to TgBotCommandModules #################
configure_file(${COMMANDS_GEN_FILE}.in ${CMAKE_BINARY_DIR}/${COMMANDS_GEN_FILE})
extend_set(SRC_LIST ${CMAKE_BINARY_DIR}/${COMMANDS_GEN_FILE})
#####################################################################

####################### Database (Protobuf) #######################
find_package(Protobuf REQUIRED)
include(proto/ProtobufCpp.cmake)
my_protobuf_generate_cpp(proto/gen PROTO_SRCS PROTO_HDRS proto/TgBotDB.proto)
add_library(TgBotDB SHARED src/Database.cpp
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
  extend_set(ABSL_LIBS ${ABSL_LIB})
endforeach()
####################### TgBotDB lib includes #######################
add_dependencies(TgBotDB protobuf_TgBotDB_ready)
target_link_libraries(TgBotDB TgBot TgBotUtils protobuf::libprotobuf ${ABSL_LIBS})
#####################################################################

################## RTCL (Run Time Command Loader) ##################
if(ENABLE_RUNTIME_COMMAND)
  add_definitions(-DRTCOMMAND_LOADER)
  extend_set(SRC_LIST src/RTCommandLoader.cpp)
  add_subdirectory(src/command_modules/runtime)
  if (WIN32)
    add_subdirectory(src/third-party/dlfcn-win32)
    set(CMAKE_DL_LIBS dl)
  endif()
endif()
#####################################################################

####################### TgBot Socket Library #######################
if (USE_UNIX_SOCKETS)
  extend_set(SRC_LIST
    src/ChatObserver.cpp
    src/SocketConnectionHandler.cpp)
  if (UNIX)
    set(SOCKET_SRC_LIST
      src/socket/SocketInterfaceUnix.cpp
      src/socket/impl/SocketInterfacePosixLocal.cpp
      src/socket/impl/SocketInterfacePosixIPv4.cpp
      src/socket/impl/SocketInterfacePosixIPv6.cpp
    )
    if (APPLE)
      extend_set(SOCKET_SRC_LIST
        src/socket/impl/SocketHelperDarwin.cpp
      )
    else()
      extend_set(SOCKET_SRC_LIST
        src/socket/impl/SocketHelperLinux.cpp
      )
    endif()
  elseif(WIN32)
    extend_set(SOCKET_SRC_LIST 
      src/socket/SocketInterfaceWindows.cpp
      src/socket/impl/SocketInterfaceWindowsLocal.cpp
      src/socket/impl/SocketInterfaceWindowsIPv4.cpp
      src/socket/impl/SocketInterfaceWindowsIPv6.cpp
      src/socket/impl/SocketHelperWindows.cpp
    )
  endif()
  extend_set(SOCKET_SRC_LIST
    src/socket/impl/SocketHelperCommon.cpp)

  if (CURL_FOUND)
    extend_set(SOCKET_SRC_LIST
      src/socket/impl/SocketHelperCommonCurl.cpp)
  else()
    extend_set(SOCKET_SRC_LIST
      src/socket/impl/SocketHelperCommonNoCurl.cpp)
  endif()
  add_library(TgBotSocket SHARED
    ${SOCKET_SRC_LIST}
    src/socket/SocketInterfaceBase.cpp
    src/socket/TgBotCommandMap.cpp)
  target_include_directories(TgBotSocket PRIVATE ${CURL_INCLUDE_DIRS})
  add_executable(${SOCKET_CLI_NAME} src/socket/TgBotSocketClient.cpp)

  target_link_libraries(${SOCKET_CLI_NAME} TgBotSocket TgBotUtils)
  target_link_lib_if_windows(${SOCKET_CLI_NAME} Ws2_32)
  if (CURL_FOUND)
    target_link_libraries(TgBotSocket ${CURL_LIBRARIES})
  endif()
  target_link_libraries(TgBotSocket TgBotUtils)
endif()
#####################################################################

################## libgit2 for accessing git in C code ##############
add_subdirectory(src/third-party/libgit2)
set(LIBGIT2_LIBS libgit2 util pcre http-parser xdiff ${ZLIB_LIBRARIES} ${OPENSSL_LIBRARIES})
if (WIN32)
  extend_set(LIBGIT2_LIBS winhttp CRYPT32 Rpcrt4 Secur32 Ws2_32)
elseif(APPLE)
  extend_set(LIBGIT2_LIBS ntlmclient iconv ${MACOS_SSL_LIBS})
else()
  extend_set(LIBGIT2_LIBS ntlmclient)
endif()
#####################################################################
################# TgBot Utilities (generic) Library #################
add_library(
  TgBotUtils SHARED
  src/ConfigManager.cpp
  src/SingleThreadCtrl.cpp
  src/SingleThreadCtrlManager.cpp
  src/ResourceManager.cpp
  src/GitData.cpp
  src/popen_wdt/popen_wdt_${TARGET_VARIANT}.c
  src/libos/libfs.cpp
  src/libos/libfs_${TARGET_VARIANT}.cpp)

target_link_libraries(TgBotUtils ${Boost_LIBRARIES} ${LIBGIT2_LIBS})
target_link_lib_if_windows(TgBotUtils shlwapi)
#####################################################################

#################### TgBot cpp (the core) Library ###################
add_subdirectory(lib)
#####################################################################

################### The Bot's main functionaility ###################
add_library(TgBotCommandModules SHARED ${SRC_LIST})
target_include_directories(TgBotCommandModules PRIVATE ${COMMANDS_CWD})
target_include_directories(TgBotCommandModules PRIVATE src/third-party/rapidjson/include)
######################## Libraries to link ########################
set(LD_LIST TgBot TgBotDB TgBotUtils ${ABSL_LIBS})
extend_set_if(LD_LIST USE_UNIX_SOCKETS TgBotSocket)
extend_set_if(LD_LIST WIN32 wsock32 Ws2_32)
extend_set_if(LD_LIST ENABLE_RUNTIME_COMMAND ${CMAKE_DL_LIBS})
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
  target_link_libraries(${MEDIA_CLI_NAME} TgBotDB TgBotSocket TgBotUtils ${ABSL_LIBS})
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
  tests/ResourceManagerTest.cpp
)
target_link_libraries(${PROJECT_TEST_NAME} gtest gtest_main gmock TgBotCommandModules)
add_test(NAME ${PROJECT_TEST_NAME} COMMAND ${PROJECT_TEST_NAME})
#####################################################################
