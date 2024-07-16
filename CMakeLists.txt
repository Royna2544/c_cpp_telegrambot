cmake_minimum_required(VERSION 3.13)
project(TgBot++ LANGUAGES CXX C)

####################### Set CMake Policy #######################
cmake_policy(SET CMP0077 NEW) # option() command prefers to do nothing
                              # when a normal variable of the given name already exists.

include(cmake/macros.cmake)
include(cmake/variables.cmake)
include(cmake/sanitizers.cmake)
################ EXE NAMES STEMMED FROM PROJECT_NAME ################
set(PROJECT_MAINEXE_NAME ${PROJECT_NAME}_main)
#####################################################################

############################## OPTIONS ##############################
option(ENABLE_RUST_SUPPORT "Enable rust support" OFF)
#####################################################################

##################### Fill in readme with cmake #####################
string(TIMESTAMP TODAY "%Y-%m-%d")
configure_file(resources/about.html.in ${CMAKE_SOURCE_DIR}/resources/about.html)
#####################################################################

################## Add a common logging init lib ####################
function(add_my_library)
  cmake_parse_arguments(TGBOT_LIBRARY
    "NO_LIBSUFFIX" # Options
    "NAME" # One-value keywords
    "SRCS;LIBS;PUBLIC_INC;PRIVATE_INC;DEPENDS;DEFS;LIBS_WIN32" # Multiple-value keywords
    ${ARGN}
  )
  if (NOT TGBOT_LIBRARY_SRCS OR NOT TGBOT_LIBRARY_NAME)
    message(FATAL_ERROR "Must specify src or name")
  endif()
  if (NOT TGBOT_LIBRARY_NO_LIBSUFFIX)
    set(TGBOT_LIBRARY_NAME TgBot${TGBOT_LIBRARY_NAME})
  endif()
  add_library(${TGBOT_LIBRARY_NAME} SHARED ${TGBOT_LIBRARY_SRCS})
  add_sanitizers(${TGBOT_LIBRARY_NAME})
  set(EXPORTCONF_DIR ${CMAKE_BINARY_DIR}/exportConfig/)
  configure_file(${CMAKE_SOURCE_DIR}/src/include/LibraryExports.h.inc
    ${EXPORTCONF_DIR}${TGBOT_LIBRARY_NAME}Exports.h)
  target_include_directories(${TGBOT_LIBRARY_NAME} PUBLIC ${TGBOT_LIBRARY_PUBLIC_INC} ${EXPORTCONF_DIR})
  target_include_directories(${TGBOT_LIBRARY_NAME} PRIVATE ${TGBOT_LIBRARY_PRIVATE_INC})
  target_link_libraries(${TGBOT_LIBRARY_NAME} absl::log ${TGBOT_LIBRARY_LIBS})
  if (WIN32)
    target_link_libraries(${TGBOT_LIBRARY_NAME} ${TGBOT_LIBRARY_LIBS_WIN32})
  endif()
  target_compile_definitions(${TGBOT_LIBRARY_NAME} PRIVATE ${TGBOT_LIBRARY_DEFS})
  if (TGBOT_LIBRARY_DEPENDS)
    add_dependencies(${TGBOT_LIBRARY_NAME} ${TGBOT_LIBRARY_DEPENDS})
  endif()
endfunction(add_my_library)

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
  src/ManagedThread.cpp
  src/RegEXHandler.cpp
  src/SpamBlocker.cpp
  src/ThreadManager.cpp
  src/TgBotWrapper.cpp
  src/RTCommandLoader.cpp
  src/stringres/StringResManager.cpp
)
#####################################################################

######################## Define SQLite3 target ######################
add_my_library(
  NAME SQLite3
  SRCS src/third-party/libsqlite3/libsqlite3/libsqlite3/sqlite3.c
  PUBLIC_INC src/third-party/libsqlite3/libsqlite3/libsqlite3/
  NO_LIBSUFFIX
)
add_subdirectory(src/database)
add_subdirectory(src/random)
add_subdirectory(src/command_modules)
add_subdirectory(src/imagep)
add_subdirectory(src/stringres)
add_subdirectory(src/logging)
add_subdirectory(src/third-party/cpp-httplib)
add_subdirectory(src/third-party/tgbot-cpp EXCLUDE_FROM_ALL)
add_subdirectory(src/third-party/googletest EXCLUDE_FROM_ALL)
add_subdirectory(src/third-party/abseil-cpp EXCLUDE_FROM_ALL)
add_subdirectory(src/third-party/protobuf EXCLUDE_FROM_ALL)
if (WIN32)
  add_subdirectory(src/third-party/dlfcn-win32 EXCLUDE_FROM_ALL)
  set(CMAKE_DL_LIBS dl)
endif()
add_subdirectory(tests)
################# Android ROM builder with C++/Python #################
if (UNIX AND NOT APPLE)
  add_subdirectory(src/android_builder)
endif()
#################################################################

########################## WebPage Server ##########################
add_my_library(
  NAME Web 
  SRCS src/web/WebServerBase.cpp
  LIBS httplib::httplib
)
extend_set(SRC_LIST src/web/TgBotWebServer.cpp)
####################################################################

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
  add_subdirectory(src/socket)
endif()
#####################################################################

################# TgBot Utilities (generic) Library #################
include(cmake/tgbotutils.cmake)
#####################################################################

################### The Bot's main functionaility ###################
set(LD_LIST TgBot TgBotDBImpl TgBotUtils TgBotWeb TgBotStringRes TgBotRandom ${CMAKE_DL_LIBS})
extend_set_if(LD_LIST USE_UNIX_SOCKETS TgBotSocket)
add_my_library(
  NAME PPImpl
  SRCS ${SRC_LIST}
  PRIVATE_INC src/third-party/rapidjson/include
  LIBS ${LD_LIST}
  DEPENDS gen_stringres_header
)
#####################################################################

################# The Bot's main launcher (program) #################
add_executable_san(${PROJECT_MAINEXE_NAME} src/main.cpp)
target_link_libraries(${PROJECT_MAINEXE_NAME} TgBotPPImpl TgBotLogInit)
#####################################################################

