cmake_minimum_required(VERSION 3.13)
project(TgBot++ LANGUAGES CXX C)

####################### Set CMake Policy #######################
cmake_policy(SET CMP0077 NEW) # option() command prefers to do nothing
                              # when a normal variable of the given name already exists.

include(CMake/macros.cmake)
include(CMake/variables.cmake)
include(CMake/sanitizers.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# Set RPATH for Unix-like systems
if(UNIX)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()
################ EXE NAMES STEMMED FROM PROJECT_NAME ################
set(PROJECT_MAINEXE_NAME ${PROJECT_NAME}_main)
#####################################################################

############################## OPTIONS ##############################
option(ENABLE_RUST_SUPPORT "Enable rust support" OFF)
option(DISABLE_SANITIZERS "Disable all sanitizers (ASan, TSan, etc)" OFF)
#####################################################################

##################### Fill in readme with cmake #####################
string(TIMESTAMP TODAY "%Y-%m-%d")
configure_file(resources/about.html.in ${CMAKE_SOURCE_DIR}/resources/about.html)
#####################################################################

################## Add a common logging init lib ####################
function(add_my_library)
  cmake_parse_arguments(TGBOT_LIBRARY
    "NO_LIBPREFIX;STATIC" # Options
    "NAME" # One-value keywords
    "SRCS;LIBS;PUBLIC_INC;PRIVATE_INC;DEPENDS;DEFS;LIBS_WIN32" # Multiple-value keywords
    ${ARGN}
  )
  if (NOT TGBOT_LIBRARY_SRCS OR NOT TGBOT_LIBRARY_NAME)
    message(FATAL_ERROR "Must specify src or name")
  endif()
  if (NOT TGBOT_LIBRARY_NO_LIBPREFIX)
    set(TGBOT_LIBRARY_NAME TgBot${TGBOT_LIBRARY_NAME})
  endif()
  if (TGBOT_LIBRARY_STATIC)
    add_library(${TGBOT_LIBRARY_NAME} STATIC ${TGBOT_LIBRARY_SRCS})
  else()
    add_library(${TGBOT_LIBRARY_NAME} SHARED ${TGBOT_LIBRARY_SRCS})
  endif()
  install(TARGETS ${TGBOT_LIBRARY_NAME} DESTINATION lib)
  set_target_properties(${TGBOT_LIBRARY_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
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

function(add_my_executable)
  cmake_parse_arguments(TGBOT_EXE
    "NO_PREFIX" # Options
    "NAME" # One-value keywords
    "SRCS;LIBS;LIBS_WIN32" # Multiple-value keywords
    ${ARGN}
  )
  if (NOT TGBOT_EXE_SRCS OR NOT TGBOT_EXE_NAME)
    message(FATAL_ERROR "Must specify src or name")
  endif()
  if (NOT TGBOT_EXE_NO_PREFIX)
    set(TGBOT_EXE_NAME ${PROJECT_NAME}_${TGBOT_EXE_NAME})
  endif()
  add_executable(${TGBOT_EXE_NAME} ${TGBOT_EXE_SRCS})
  install(TARGETS ${TGBOT_EXE_NAME} DESTINATION bin)
  add_sanitizers(${TGBOT_EXE_NAME})
  target_link_libraries(${TGBOT_EXE_NAME} absl::log TgBotLogInit ${TGBOT_EXE_LIBS})
  if (WIN32)
    target_link_libraries(${TGBOT_EXE_NAME} ${TGBOT_EXE_LIBS_WIN32})
  endif()
endfunction(add_my_executable)

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
find_package(libgit2 REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Protobuf)
include_directories(${Boost_INCLUDE_DIR})

# If we couldn't find protobuf...
if (NOT Protobuf_FOUND)
  # Then we can just include source-abseil
  message(STATUS "Using abseil-cpp as submodule")
  set(ABSL_PROPAGATE_CXX_STD ON)
  add_subdirectory(src/third-party/abseil-cpp)
else()
  # If there was, then we can cause collision with the system abseil
  find_package(absl REQUIRED)
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
  src/RegEXHandler.cpp
  src/RegEXHandlerImpl.cpp
  src/SpamBlocker.cpp
  src/RTCommandLoader.cpp
  src/web/TgBotWebServer.cpp
)
#####################################################################

######################## Define SQLite3 target ######################
add_my_library(
  NAME CommandLine
  SRCS src/CommandLine.cpp
)
add_my_library(
  NAME PPImpl_shared_deps
  SRCS 
    src/Authorization.cpp
    src/ManagedThread.cpp
    src/ThreadManager.cpp
    src/TgBotWrapper.cpp
  LIBS
    TgBot
    TgBotStringResManager
    TgBotRandom
    TgBotDBImpl
)

add_subdirectory(src/android_builder)
add_subdirectory(src/database)
add_subdirectory(src/random)
add_subdirectory(src/command_modules)
add_subdirectory(src/imagep)
add_subdirectory(src/stringres)
add_subdirectory(src/logging)
add_subdirectory(src/web)
add_subdirectory(src/third-party/cpp-httplib)
add_subdirectory(src/third-party/tgbot-cpp)
if (WIN32)
  add_subdirectory(src/third-party/dlfcn-win32)
  set(CMAKE_DL_LIBS dl)
endif()
add_subdirectory(tests)
#################################################################

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
include(CMake/tgbotutils.cmake)
#####################################################################

################### The Bot's main functionaility ###################
set(LD_LIST TgBot TgBotUtils TgBotWeb absl::status
            TgBotStringResManager TgBotPPImpl_shared_deps ${CMAKE_DL_LIBS})
extend_set_if(LD_LIST USE_UNIX_SOCKETS TgBotSocket)
add_my_library(
  NAME PPImpl
  SRCS ${SRC_LIST}
  PRIVATE_INC src/third-party/rapidjson/include
  LIBS ${LD_LIST}
)
#####################################################################

################# The Bot's main launcher (program) #################
add_my_executable(
  NAME main
  SRCS src/main.cpp
  LIBS TgBotPPImpl TgBotLogInit
)
#####################################################################

install(DIRECTORY resources/ DESTINATION resources FILES_MATCHING  PATTERN "*"
  PATTERN "*.in" EXCLUDE)
