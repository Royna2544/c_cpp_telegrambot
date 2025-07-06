cmake_minimum_required(VERSION 3.14)
project(TgBot++ LANGUAGES CXX C)

# CMake 3.25 added LINUX variable
if(CMAKE_VERSION VERSION_LESS "3.25")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX
        TRUE
        CACHE BOOL "Linux platform")
  endif()
endif()

# ###################### Set CMake Policy #######################
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# ############################# OPTIONS ##############################
option(ENABLE_RUST_SUPPORT "Enable rust support" OFF)
option(SANITIZER_CONFIG "Sanitizers config for debug builds (ASan, TSan, etc)"
       "ASan")
option(BUILD_SHARED_LIBS "Build some dependent libraries as shared" OFF)
option(ENABLE_TESTING "Enable testing" OFF)
# ##############################################################################

# ##############################################################################
# Make vcpkg copy the dlls required.
set(VCPKG_APPLOCAL_DEPS
    ON
    CACHE BOOL "Copy VCPKG's dlls to exe directory" FORCE)
# On installation too.
set(X_VCPKG_APPLOCAL_DEPS_INSTALL
    ON
    CACHE BOOL "Copy VCPKG's dlls to install" FORCE)
# ##############################################################################

# ######################## C++ Configuration #########################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# ##############################################################################

# Check PIE support
include(CheckPIESupported)
check_pie_supported()
if(CMAKE_CXX_LINK_PIE_SUPPORTED AND CMAKE_C_LINK_PIE_SUPPORTED)
  message(STATUS "Enabling PIE (Position Independent Executable) support")
  set(PIE_SUPPORTED
      TRUE
      CACHE BOOL "Whether PIE is supported")
else()
  message(STATUS "PIE (Position Independent Executable) not enabled")
endif()

# Sanitizers configuration
set(SANITIZER_FLAG)

if(NOT SANITIZER_CONFIG)
  message(STATUS "No sanitizers enabled")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  message(STATUS "Disabling sanitizers as release build")
  # MSYS2 is special - They don't have any sanitizers
elseif(MINGW OR MSYS)
  message(STATUS "MSYS2 detected - Disabling sanitizers")
elseif(SANITIZER_CONFIG STREQUAL "ASan")
  message(STATUS "Address sanitizer enabled")
  if(APPLE) # On apple, leak sanitizer is not implemented
    set(SANITIZER_FLAG "-fsanitize=address")
  elseif(MSVC)
    set(SANITIZER_FLAG "/fsanitize=address")
  else()
    set(SANITIZER_FLAG "-fsanitize=address,leak")
  endif()
elseif(SANITIZER_CONFIG STREQUAL "UBSan")
  if(MSVC)
    set(SANITIZER_FLAG "/fsanitize=undefined")
  else()
    set(SANITIZER_FLAG "-fsanitize=undefined")
  endif()
elseif(SANITIZER_CONFIG STREQUAL "TSan")
  message(STATUS "Thread sanitizer enabled")
  if(MSVC)
    set(SANITIZER_FLAG "/fsanitize=thread")
  else()
    set(SANITIZER_FLAG "-fsanitize=thread")
  endif()
elseif(SANITIZER_CONFIG STREQUAL "MSan")
  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "Memory sanitizer enabled")
    set(SANITIZER_FLAG "-fsanitize=memory")
  else()
    message(WARNING "Memory sanitizer is only supported with Clang")
  endif()
else()
  message(WARNING "Unknown sanitizer configuration: ${SANITIZER_CONFIG}")
endif()

# Compiler specific settings
set(GLOBAL_COMPILE_OPTIONS)
set(GLOBAL_DEFINITIONS)
set(GLOBAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/include
                        ${CMAKE_SOURCE_DIR}/src/)

# Test proper c++20 jthread, stop_token
set(_jthread_test_source
    "
#include <thread>
#include <stop_token>

int main() {
   std::stop_token st;
   std::jthread jt;
}
")

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("${_jthread_test_source}" CXX20_JTHREAD_COMPILES)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # Clang may opt in to experimental library
  if(NOT CXX20_JTHREAD_COMPILES)
    set(CMAKE_REQUIRED_FLAGS "-fexperimental-library")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    check_cxx_source_compiles("${_jthread_test_source}"
                              CXX20_JTHREAD_COMPILES_EXPERIMENTAL)
    if(CXX20_JTHREAD_COMPILES_EXPERIMENTAL)
      set(CXX20_JTHREAD_COMPILES TRUE)
      list(APPEND GLOBAL_COMPILE_OPTIONS -fexperimental-library)
      message(STATUS "Using experimental library")
    endif()
    unset(CMAKE_REQUIRED_FLAGS)
    unset(CMAKE_TRY_COMPILE_TARGET_TYPE)
  endif()

  # Prefer LLD
  list(APPEND CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
  list(APPEND CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld")
endif()

if(NOT CXX20_JTHREAD_COMPILES)
  message(FATAL_ERROR "Sorry, your compiler is too old")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT LTO_SUPPORTED)
  if(LTO_SUPPORTED)
    message(STATUS "Enabling LTO (Link time optimization)")
  endif()
endif()
if(WIN32)
  list(APPEND GLOBAL_DEFINITIONS _WIN32_WINNT=0x0A00)
  if(MSVC)
    list(APPEND GLOBAL_COMPILE_OPTIONS /utf-8)
    list(APPEND GLOBAL_DEFINITIONS NOMINMAX)
  endif()
endif()
if(NOT MSVC)
  list(APPEND GLOBAL_COMPILE_OPTIONS
       -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)
  list(APPEND GLOBAL_COMPILE_OPTIONS -Wno-unused-parameter)
endif()

# Build Type
list(APPEND GLOBAL_DEFINITIONS BUILD_TYPE_STR="${CMAKE_BUILD_TYPE}")

# Library exports
set(EXPORTCONF_DIR "${CMAKE_BINARY_DIR}/exportConfig")

# #################### Fill in readme with cmake #####################
string(TIMESTAMP TODAY "%Y-%m-%d %H:%M:%S UTC" UTC)

find_package(Git REQUIRED) # It will obviously have
function(git_execute_proc)
  cmake_parse_arguments(
    GIT_PROC
    "" # Option
    "NAME;VAR" # Single
    "COMMAND" # Multiple
    ${ARGN})
  if(NOT
     (GIT_PROC_NAME
      AND GIT_PROC_VAR
      AND GIT_PROC_COMMAND))
    message(SEND_ERROR "Missing arguments")
  endif()

  # Run the git command to get the commit ID or other info
  execute_process(
    COMMAND ${GIT_EXECUTABLE} ${GIT_PROC_COMMAND}
    OUTPUT_VARIABLE GIT_PROC_OUT
    ERROR_VARIABLE GIT_PROC_ERR
    RESULT_VARIABLE GIT_PROC_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Check if the git command was successful
  if(GIT_PROC_RESULT EQUAL 0)
    set(${GIT_PROC_VAR}
        ${GIT_PROC_OUT}
        PARENT_SCOPE)
    message(STATUS "Git ${GIT_PROC_NAME}: ${GIT_PROC_OUT}")
  else()
    set(${GIT_PROC_VAR}
        "-"
        PARENT_SCOPE)
    message(WARNING "Error retrieving Git ${GIT_PROC_NAME}: ${GIT_PROC_ERR}")
  endif()
endfunction()

git_execute_proc(
  COMMAND
  rev-parse
  HEAD
  NAME
  "commit-id"
  VAR
  GIT_COMMIT_ID)

git_execute_proc(
  COMMAND
  log
  -1
  --pretty=%B
  NAME
  "commit-message"
  VAR
  GIT_COMMIT_MESSAGE)

git_execute_proc(
  COMMAND
  remote
  get-url
  origin
  NAME
  "origin-url"
  VAR
  GIT_ORIGIN_URL)

configure_file(resources/about.html.in ${CMAKE_SOURCE_DIR}/resources/about.html)
configure_file(src/include/GitBuildInfo.hpp.inc
               ${CMAKE_SOURCE_DIR}/src/include/GitBuildInfo.hpp)
# ##############################################################################

# Optional components group
add_custom_target(optional_all)

# Library install directory
include(GNUInstallDirs)
if(WIN32)
  set(LIBRARY_INSTALL_DIR ${CMAKE_INSTALL_BINDIR})
else()
  set(LIBRARY_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
endif()

# ################# Declare common macros ####################
# tgbot_common_target(<target-name> TYPE <LIBRARY|EXECUTABLE> SRCS <src1>
# [<src2> …] [NO_PREFIX] [PUBLIC_INC …] [PRIVATE_INC …] [PUBLIC_LIBS …]
# [PRIVATE_LIBS …] [DEFS …]
function(tgbot_common_target TARGET_NAME)
  # ---------- argument parsing (shared) ----------
  cmake_parse_arguments(
    TGBOT "NO_PREFIX" "TYPE;RELATION"
    "SRCS;PUBLIC_INC;PRIVATE_INC;PUBLIC_LIBS;PRIVATE_LIBS;INTERFACE_LIBS;DEFS"
    ${ARGN})

  # ---------- generic properties & tooling ----------
  target_include_directories(
    ${TARGET_NAME}
    PUBLIC ${TGBOT_PUBLIC_INC} ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${TGBOT_PRIVATE_INC} ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/src/include ${EXPORTCONF_DIR})

  target_compile_definitions(${TARGET_NAME} PUBLIC ${TGBOT_DEFS})
  target_link_libraries(
    ${TARGET_NAME}
    PUBLIC ${TGBOT_PUBLIC_LIBS}
    PRIVATE ${TGBOT_PRIVATE_LIBS} fmt::fmt-header-only
    INTERFACE ${TGBOT_INTERFACE_LIBS})

  # Set RPATH for Unix-like systems
  if(UNIX)
    # bin/main bin/cli lib/libTgBot.so
    set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH
                                                    TRUE)
    set_target_properties(${TARGET_NAME} PROPERTIES INSTALL_RPATH
                                                    "$ORIGIN/../lib;$ORIGIN/..")
  endif()

  # Output directory
  set_target_properties(
    ${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                              ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
  set_target_properties(
    ${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY
                              ${CMAKE_BINARY_DIR}/${LIBRARY_INSTALL_DIR})

  # sanitizers / PIE / LTO
  if(SANITIZER_FLAG)
    target_compile_options(${TARGET_NAME} PRIVATE ${SANITIZER_FLAG})
    target_link_options(${TARGET_NAME} PRIVATE ${SANITIZER_FLAG})
  endif()
  if(PIE_SUPPORTED)
    set_target_properties(${TARGET_NAME} PROPERTIES POSITION_INDEPENDENT_CODE
                                                    ON)
  endif()
  if(LTO_SUPPORTED)
    set_target_properties(${TARGET_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION
                                                    ON)
  endif()

  # Global variables
  target_compile_definitions(${TARGET_NAME} PRIVATE ${GLOBAL_DEFINITIONS})
  target_compile_options(${TARGET_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
  target_include_directories(${TARGET_NAME} PRIVATE ${GLOBAL_INCLUDE_DIRS})
endfunction()

function(tgbot_library)
  cmake_parse_arguments(LIB "NO_LIBPREFIX;STATIC;NO_HIDE" "NAME"
                        "SRCS;PUBLIC_INC;PRIVATE_INC;DEFS;LIBS" ${ARGN})

  # Collect public & private libs exactly once
  set(PUBLIC_LIBS ${LIB_LIBS})
  set(PRIVATE_LIBS absl::log)

  if(LIB_STATIC)
    add_library(${LIB_NAME} STATIC ${LIB_SRCS})
    list(APPEND PUBLIC_LIBS ${PRIVATE_LIBS})
    set(PRIVATE_LIBS)
  else()
    add_library(${LIB_NAME} SHARED ${LIB_SRCS})
    list(APPEND LIB_PUBLIC_INC ${EXPORTCONF_DIR})
  endif()

  if(NOT LIB_NO_LIBPREFIX)
    if (WIN32)
       set_target_properties(${LIB_NAME} PROPERTIES PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}Telegram.Bot.")
    else()
       set_target_properties(${LIB_NAME} PROPERTIES PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}Tgbot")
    endif()
  endif()

  if(NOT LIB_NO_HIDE)
    set_target_properties(${LIB_NAME} PROPERTIES CXX_VISIBILITY_PRESET hidden
                                                 VISIBILITY_INLINES_HIDDEN YES)
  endif()

  tgbot_common_target(
    ${LIB_NAME}
    PUBLIC_INC
    ${LIB_PUBLIC_INC}
    PRIVATE_INC
    ${LIB_PRIVATE_INC}
    PUBLIC_LIBS
    ${PUBLIC_LIBS}
    PRIVATE_LIBS
    ${PRIVATE_LIBS}
    DEFS
    ${LIB_DEFS})

  if(NOT LIB_STATIC)
    file(MAKE_DIRECTORY "${EXPORTCONF_DIR}")

    include(GenerateExportHeader)
    generate_export_header(${LIB_NAME} BASE_NAME ${LIB_NAME} EXPORT_FILE_NAME
                           "${EXPORTCONF_DIR}/${LIB_NAME}Exports.h")
    install(TARGETS ${LIB_NAME} DESTINATION ${LIBRARY_INSTALL_DIR})
  endif()
endfunction()

function(tgbot_exe)
  cmake_parse_arguments(EXE "NO_PREFIX;OPTIONAL;TEST;NO_MAIN_WRAPPER"
                        "NAME;RELATION" "SRCS;LIBS" ${ARGN})

  # Tests get "test_" prefix
  if(EXE_TEST)
    set(TARGET_NAME test_${EXE_NAME})
  else()
    set(TARGET_NAME ${EXE_NAME})
  endif()

  if(NOT EXE_NO_MAIN_WRAPPER)
    list(APPEND EXE_SRCS ${CMAKE_SOURCE_DIR}/src/logging/WrapMain.cpp)
    list(APPEND EXE_LIBS LogInit absl::log)
  endif()

  if(EXE_OPTIONAL)
    add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL ${EXE_SRCS})
    add_dependencies(optional_all ${TARGET_NAME})
  else()
    add_executable(${TARGET_NAME} ${EXE_SRCS})
  endif()
  if(NOT EXE_NO_PREFIX)
    set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "${PROJECT_NAME}_")
  endif()

  tgbot_common_target(${TARGET_NAME} PUBLIC_LIBS ${EXE_LIBS})

  if(EXE_TEST)
    if(MSVC AND BUILD_SHARED_LIBS)
      # https://github.com/google/googletest/issues/292
      target_compile_definitions(${TARGET_NAME}
                                 PRIVATE GTEST_LINKED_AS_SHARED_LIBRARY)
    endif()

    # ---------- extra test registration ----------
    add_test(NAME TestSuite_${TARGET_NAME} COMMAND ${TARGET_NAME})
  elseif(NOT EXE_OPTIONAL)
    if(NOT EXE_RELATION)
      message(
        SEND_ERROR
          "Please specify RELATION for ${TARGET_NAME}. Valid: Main | Socket | Database"
      )
    endif()
    install(TARGETS ${TARGET_NAME} COMPONENT App${EXE_RELATION})
  endif()
endfunction()

# ########################### RUST support ###########################
if(ENABLE_RUST_SUPPORT)
  add_subdirectory(src/third-party/corrosion)
  corrosion_import_crate(MANIFEST_PATH src/rust/stringcalc/Cargo.toml)
endif()
# ##############################################################################

# ############################ Check lib #############################
find_package(Protobuf)
find_package(fmt REQUIRED)
find_package(jsoncpp REQUIRED) # Required by tgbot-cpp anyways...
find_package(CURL REQUIRED)
# If we couldn't find protobuf.
if(NOT Protobuf_FOUND)
  if(MSVC)
    # If MSVC, use vcpkg's absl always.
    find_package(absl REQUIRED)
  else()
    # Then we can just include source-abseil
    message(STATUS "Using abseil-cpp as submodule")
    set(ABSL_PROPAGATE_CXX_STD ON)
    set(ABSL_BUILD_TEST_HELPERS OFF)
    set(ABSL_ENABLE_INSTALL ON)
    set(BUILD_SHARED_LIBS ON)
    add_subdirectory(src/third-party/abseil-cpp)
    unset(BUILD_SHARED_LIBS)
  endif()
else()
  # If there was, then we can cause collision with the system abseil
  find_package(absl REQUIRED)
endif()
# ##############################################################################

# Configure Ccache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  message(STATUS "Found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
else()
  message(STATUS "Could NOT find ccache (this is NOT an error)")
endif()

# ############################ Platforms #############################
if(WIN32)
  set(TARGET_VARIANT windows)
else()
  set(TARGET_VARIANT posix)
endif()
# ##############################################################################

# ####################### Include subprojects ######################
# dlfcn (libdl)
if(WIN32)
  add_subdirectory(src/third-party/dlfcn-win32)
  set(CMAKE_DL_LIBS dl)
elseif(ANDROID)
  set(CMAKE_DL_LIBS)
else()
  find_library(DL dl)
  add_library(dl SHARED IMPORTED)
  set_target_properties(dl PROPERTIES IMPORTED_LOCATION "${DL}")
endif()

# Used by libzstd in cpptrace
set(ANDROID_PLATFORM_LEVEL 35)

add_subdirectory(src/api)
add_subdirectory(src/database)
add_subdirectory(src/random)
add_subdirectory(src/imagep)
add_subdirectory(src/logging)
add_subdirectory(src/socket)
add_subdirectory(src/utils)
add_subdirectory(src/web)
add_subdirectory(src/libos)
add_subdirectory(src/third-party/tgbot-cpp)
add_subdirectory(src/third-party/cpptrace)
add_subdirectory(src/third-party/fruit)
add_subdirectory(src/third-party/sol2)
find_package(Python3 QUIET)
if(Python3_FOUND)
  message(STATUS "Enabling cpp-httplib compilation")
  set(HTTPLIB_COMPILE ON)
endif()
add_subdirectory(src/third-party/cpp-httplib)

# Tests
if(ENABLE_TESTING)
  if(MSVC)
    # Always use source built googletest for MSVC
    # https://github.com/google/googletest/issues/4357
    set(gtest_force_shared_crt TRUE)
    set(INSTALL_GTEST FALSE)
    add_subdirectory(src/third-party/googletest)
  else()
    find_package(GTest REQUIRED)
  endif()
  enable_testing()
  add_subdirectory(tests)
else()
  message(STATUS "Disabling tests")
endif()
# ################## The Bot's main functionaility ###################
tgbot_library(
  NAME
  Regex
  SRCS
  src/global_handlers/RegEXHandler.cpp
  LIBS
  absl::status
  STATIC)

tgbot_library(
  NAME
  SocketComponents
  SRCS
  src/global_handlers/SpamBlocker.cpp
  src/global_handlers/ChatObserver.cpp
  src/socket/bot/SocketDataHandler.cpp
  src/socket/bot/SocketInterface.cpp
  LIBS
  TgBot
  DBImpl
  Socket
  STATIC)

tgbot_library(NAME ThreadManager SRCS src/ManagedThread.cpp
              src/ThreadManager.cpp STATIC)
# ##############################################################################

# ################ The Bot's main launcher (program) #################

tgbot_exe(
  NAME
  main
  SRCS
  src/main.cpp
  src/global_handlers/SpamBlockManager.cpp
  src/ml/ChatDataCollector.cpp
  src/web/TgBotWebServer.cpp
  src/logging/LoggingServer.cpp
  LIBS
  DBLoading
  fruit
  TgBot
  Utils
  Web
  Random
  restartfmt_parser
  Regex
  SocketComponents
  sighandler
  Api
  ThreadManager
  RELATION
  Main)

# Add an icon
if(WIN32)
  target_sources(main PRIVATE resources/photo/resource.rc)
endif()

if(UNIX)
  tgbot_exe(NAME maind SRCS src/mainDaemon.cpp RELATION Main)
endif()

# ##############################################################################
# Resource dirs
install(
  DIRECTORY resources/
  DESTINATION share/${PROJECT_NAME}
  FILES_MATCHING
  PATTERN "*"
  PATTERN "*.in" EXCLUDE)
file(COPY resources/ DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME})

# Set CPack configurations
set(CPACK_PACKAGE_NAME "TgBot++")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Telegram bot coded in C++")
set(CPACK_PACKAGE_CONTACT "support@example.com")
set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TgBot C++ server ${CPACK_PACKAGE_VERSION}")

if(WIN32)
  # NSIS
  list(APPEND CPACK_GENERATOR NSIS)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/resources/photo/icon.ico)
  set(CPACK_NSIS_MUI_UNIICON ${CMAKE_SOURCE_DIR}/resources/photo/uninstall.ico)
  set(CPACK_NSIS_DISPLAY_NAME "Telegram Bot Server")
  set(CPACK_NSIS_WELCOME_TITLE "Welcome! Actually, no")
  set(CPACK_NSIS_BRANDING_TEXT "Created with CMake ${CMAKE_VERSION}")
  set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
  set(CPACK_NSIS_INSTALLED_ICON_NAME
      "${CMAKE_BINARY_DIR}/bin/${PROJECT_NAME}_main.exe")
  set(CPACK_NSIS_MANIFEST_DPI_AWARE TRUE)
endif()

# Include CPack
include(CPack)

cpack_add_component_group(
  OptionalCli
  DISPLAY_NAME "Optional Cli tools"
  DESCRIPTION "Additional CLIs."
  EXPANDED # Expand the group by default in the installer UI
)

cpack_add_component(
  AppMain
  DISPLAY_NAME "Main Application"
  DESCRIPTION "The main server application."
  REQUIRED # Ensures this component is always installed
)

cpack_add_component(
  AppSocket
  DISPLAY_NAME "Socket Utils"
  DESCRIPTION "Socket connection clients."
  GROUP "OptionalCli")

cpack_add_component(
  AppDatabase
  DISPLAY_NAME "Database Utils"
  DESCRIPTION "Database manipulation utils."
  GROUP "OptionalCli")

# Include last for CPack
add_subdirectory(src/command_modules)
