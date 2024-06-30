cmake_minimum_required(VERSION 3.13)
project(TgBot++ LANGUAGES CXX C)

####################### Set CMake Policy #######################
cmake_policy(SET CMP0077 NEW) # option() command prefers to do nothing
                              # when a normal variable of the given name already exists.

include(cmake/macros.cmake)
include(cmake/variables.cmake)
include(cmake/sanitizers.cmake)
################ EXE NAMES STEMMED FROM PROJECT_NAME ################
set(MEDIA_CLI_NAME ${PROJECT_NAME}_MediaCli)
set(PROJECT_MAINEXE_NAME ${PROJECT_NAME}_main)
set(PROJECT_TEST_NAME ${PROJECT_NAME}_test)
set(LOGCAT_NAME ${PROJECT_NAME}_logcat)
#####################################################################

############################## OPTIONS ##############################
option(ENABLE_RUNTIME_COMMAND "Enable runtime loader of commands" ON)
option(ENABLE_RUST_SUPPORT "Enable rust support" OFF)
################# Detecting x86/x86_64 Architecture ################
if(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86_64|AMD64|amd64" AND NOT MSVC)
  add_compile_options(-mrdrnd)
endif()
#####################################################################

##################### Fill in readme with cmake #####################
string(TIMESTAMP TODAY "%Y-%m-%d")
configure_file(resources/about.html.in ${CMAKE_SOURCE_DIR}/resources/about.html)
#####################################################################

#################### TgBot cpp (the core) Library ###################
add_subdirectory(src/third-party/tgbot-cpp EXCLUDE_FROM_ALL)
#####################################################################

########################## ABSEIL log deps ##########################
add_subdirectory(src/third-party/googletest EXCLUDE_FROM_ALL)
add_subdirectory(src/third-party/abseil-cpp EXCLUDE_FROM_ALL)
add_subdirectory(src/third-party/protobuf EXCLUDE_FROM_ALL)
if (WIN32)
add_subdirectory(src/third-party/dlfcn-win32 EXCLUDE_FROM_ALL)
endif()
################## Add a common logging init lib ####################
add_library_san(TgBotLogInit SHARED src/logging/AbslLogInit.cpp)
target_include_directories(TgBotLogInit PUBLIC src/logging/)
target_link_libraries(TgBotLogInit absl::log_initialize absl::log absl::log_sink
  absl::log_sink_registry absl::str_format)
#####################################################################
link_libraries(absl::log)
#####################################################################

include(cmake/tgbotpng.cmake)
############################ RUST support ###########################
if (ENABLE_RUST_SUPPORT)
  add_subdirectory(src/third-party/corrosion)
  corrosion_import_crate(MANIFEST_PATH src/rust/stringcalc/Cargo.toml)
endif()
#####################################################################

####################### Global compiler flags #######################
if (NOT MSVC)
  add_compile_options(
    -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)  
endif()
if (WIN32)
  # Windows 10  
  add_definitions(-D_WIN32_WINNT=0x0A00)
  add_compile_definitions(-DWINDOWS_BUILD)
endif()
#####################################################################

################### Check Windows's AF_UNIX support ###################
################# Versions later than Win10 RS3 does #################
if (WIN32)
  perform_test(USE_UNIX_SOCKETS)
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
if (${CURL_FOUND})
  add_definitions(-DHAVE_CURL)
endif()
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
  src/BotMessageUtils.cpp
  src/ManagedThread.cpp
  src/RegEXHandler.cpp
  src/ResourceManager.cpp
  src/SpamBlocker.cpp
  src/ThreadManager.cpp
  src/TimerImpl.cpp
  src/command_modules/CommandModules.cpp
  src/command_modules/compiler/Base.cpp
  src/command_modules/compiler/Bash.cpp
  src/command_modules/compiler/CCpp.cpp
  src/command_modules/compiler/Generic.cpp
  src/command_modules/compiler/Helper.cpp
  src/database/bot/TgBotDatabaseImpl.cpp
  src/libos/libsighandler_impl.cpp
  src/libos/libsighandler_${TARGET_VARIANT}.cpp
  src/popen_wdt/popen_wdt.c
  src/popen_wdt/popen_wdt_${TARGET_VARIANT}.c
  src/random/RandomNumberGenerator.cpp
)
#####################################################################

########## Generate commands modules list in compile time (python) ##########
find_package (Python3 COMPONENTS Interpreter REQUIRED)
set(GENERATE_CMDMODULE_STRINGS ${CMAKE_SOURCE_DIR}/resources/scripts/command_modules_helper.py)

macro(get_str_and_assign _output_var)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${GENERATE_CMDMODULE_STRINGS} ${ARGN}
        OUTPUT_VARIABLE ${_output_var}
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
endmacro()
get_str_and_assign(CMD_MODULES_SOURCES_LIST get_cmd_filenames ${CMAKE_SOURCE_DIR}/resources/commands_list.txt)
get_str_and_assign(CMD_MODULES_LIST get_cmd_names ${CMAKE_SOURCE_DIR}/resources/commands_list.txt)
get_str_and_assign(COMMANDS_MODULES_PTR gen_ptr ${CMD_MODULES_LIST})
get_str_and_assign(COMMANDS_MODULES_DECL gen_decl ${CMD_MODULES_LIST})
########## Generate commands modules list in compile time (cpp) ##########
foreach(lib ${CMD_MODULES_SOURCES_LIST})
  extend_set(SRC_LIST src/command_modules/${lib}.cpp)
endforeach()
#################### Include to TgBot++ #################
set(COMMANDS_GEN_FILE src/command_modules/gen/cmds.gen.cpp)
configure_file(${COMMANDS_GEN_FILE}.in ${CMAKE_BINARY_DIR}/${COMMANDS_GEN_FILE})
extend_set(SRC_LIST ${CMAKE_BINARY_DIR}/${COMMANDS_GEN_FILE})
#####################################################################
include(cmake/database.cmake)

####################### TgBotStringRes lib  ######################
extend_set(SRC_LIST src/stringres/StringResManager.cpp)
include(cmake/tgbotstringres.cmake)
#################################################################

################## RTCL (Run Time Command Loader) ##################
if(ENABLE_RUNTIME_COMMAND)
  add_definitions(-DRTCOMMAND_LOADER)
  extend_set(SRC_LIST src/RTCommandLoader.cpp)
  add_subdirectory(src/command_modules/runtime)
  if (WIN32)
    set(CMAKE_DL_LIBS dl)
  endif()
endif()
#####################################################################

########################## WebPage Server ##########################
add_subdirectory(src/third-party/cpp-httplib)
add_library_san(TgBotWeb SHARED src/web/WebServerBase.cpp)
target_link_libraries(TgBotWeb httplib::httplib)
extend_set(SRC_LIST src/web/TgBotWebServer.cpp)
####################################################################

########################## Logcat Client ##########################
if (USE_UNIX_SOCKETS)
  add_executable_san(${LOGCAT_NAME} src/logging/LoggingClient.cpp)
  target_link_libraries(${LOGCAT_NAME} TgBotSocket TgBotLogInit)
endif()
####################### TgBot Socket Library #######################
if (USE_UNIX_SOCKETS)
  set(SOCKET_SRC_INTERFACE src/socket/interface)
  extend_set(SRC_LIST
    src/ChatObserver.cpp
    src/logging/LoggingServer.cpp
    ${SOCKET_SRC_INTERFACE}/impl/bot/SocketDataHandler.cpp
    ${SOCKET_SRC_INTERFACE}/impl/bot/TgBotSocketInterface.cpp
    ${SOCKET_SRC_INTERFACE}/impl/backends/ServerBackend.cpp
    ${SOCKET_SRC_INTERFACE}/impl/backends/ServerBackend_${TARGET_VARIANT}.cpp)
  include(cmake/tgbotsocket.cmake)
endif()
#####################################################################

################# TgBot Utilities (generic) Library #################
include(cmake/tgbotutils.cmake)
#####################################################################

################### The Bot's main functionaility ###################
add_library_san(${PROJECT_NAME} SHARED ${SRC_LIST})
target_include_directories(${PROJECT_NAME} PRIVATE src/third-party/rapidjson/include)
target_include_directories(${PROJECT_NAME} PUBLIC ${STRINGRES_GENHDR_DIR})
######################## Libraries to link ########################
set(LD_LIST TgBot TgBotDB TgBotUtils TgBotWeb TgBotStringRes TgBotImgProc)
extend_set_if(LD_LIST USE_UNIX_SOCKETS TgBotSocket)
extend_set_if(LD_LIST WIN32 wsock32 Ws2_32)
extend_set_if(LD_LIST ENABLE_RUNTIME_COMMAND ${CMAKE_DL_LIBS})
target_link_libraries(${PROJECT_NAME} ${LD_LIST})
add_dependencies(${PROJECT_NAME} gen_stringres_header)
#####################################################################

################# The Bot's main launcher (program) #################
add_executable_san(${PROJECT_MAINEXE_NAME} src/main.cpp)
target_link_libraries(${PROJECT_MAINEXE_NAME} ${PROJECT_NAME} TgBotLogInit
  absl::log_sink absl::log_sink_registry)
#####################################################################

############### Utility Programs (Send Media to chat) ################
if (USE_UNIX_SOCKETS)
  add_executable_san(${MEDIA_CLI_NAME} src/database/utils/SendMediaToChat.cc)
  target_link_libraries(${MEDIA_CLI_NAME} TgBotDBImpl TgBotSocket TgBotLogInit)
  target_link_lib_if_windows(${MEDIA_CLI_NAME} Ws2_32)
endif()
#####################################################################

include(cmake/tests.cmake)
