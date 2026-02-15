cmake_minimum_required(VERSION 3.14)
project(Glider LANGUAGES CXX C VERSION 1.4.0 DESCRIPTION "A Telegram bot coded in C++")

if(CMAKE_VERSION VERSION_LESS "3.25")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX
        TRUE
        CACHE BOOL "Linux platform")
  endif()
endif()

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# ###################### Set CMake Policy #######################
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# ############################# OPTIONS ##############################
option(TGBOTCPP_RUST_MODULES "TgBot-Cpp: Enable rust support" OFF)
option(TGBOTCPP_BUILD_TESTS "TgBot-Cpp: Build the test suite" OFF)
option(TGBOTCPP_CROSS_COMPILING "TgBot-Cpp: Cross compiling" OFF)
option(BUILD_SHARED_LIBS "Build some dependent libraries as shared" OFF)

include(CMakeDependentOption)
cmake_dependent_option(TGBOTCPP_AUTODETECT_CROSS_COMPILING "TgBot-Cpp: Auto-detect cross compiling" ON "NOT TGBOTCPP_CROSS_COMPILING" OFF)

if (TGBOTCPP_CROSS_COMPILING)
  option(TGBOTCPP_CROSS_COMPILE_INSTALL_PATH "TgBot-Cpp: Install prefix for cross-compiling" "/usr/")
  option(TGBOTCPP_CROSS_COMPILE_CC "TgBot-Cpp: C compiler available on remote host" "/usr/bin/cc")
  option(TGBOTCPP_CROSS_COMPILE_CXX "TgBot-Cpp: C++ compiler available on remote host" "/usr/bin/c++")
  option(TGBOTCPP_CROSS_COMPILE_PYTHON "TgBot-Cpp: Python executable available on remote host" "/usr/bin/python3")
endif()
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
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# ##############################################################################

# Check if we're cross-compiling
if(TGBOTCPP_AUTODETECT_CROSS_COMPILING)
  if(CMAKE_SYSTEM_PROCESSOR AND NOT CMAKE_SYSTEM_PROCESSOR STREQUAL CMAKE_HOST_SYSTEM_PROCESSOR)
    set(TGBOTCPP_CROSS_COMPILING TRUE)
    message(STATUS "Auto-detected cross-compiling environment (Host: ${CMAKE_HOST_SYSTEM_PROCESSOR}, Target: ${CMAKE_SYSTEM_PROCESSOR})")
  else()
    set(TGBOTCPP_CROSS_COMPILING FALSE)
    message(STATUS "Auto-detected native compiling environment (Arch: ${CMAKE_SYSTEM_PROCESSOR})")
  endif()
else()
  if(TGBOTCPP_CROSS_COMPILING)
    message(STATUS "Cross-compiling environment (manually set)")
  else()
    message(STATUS "Native compiling environment (manually set)")
  endif()
endif()

if (TGBOTCPP_CROSS_COMPILING)
  message(STATUS "Cross-compiling: Showing config")
  message(STATUS "  Install path for cross-compiling tools: ${TGBOTCPP_CROSS_COMPILE_INSTALL_PATH}")
  message(STATUS "  C compiler on remote host: ${TGBOTCPP_CROSS_COMPILE_CC}")
  message(STATUS "  C++ compiler on remote host: ${TGBOTCPP_CROSS_COMPILE_CXX}")
  message(STATUS "  Python executable on remote host: ${TGBOTCPP_CROSS_COMPILE_PYTHON}")
endif()

# Check PIE support
include(CheckPIESupported)
check_pie_supported()
if(CMAKE_CXX_LINK_PIE_SUPPORTED AND CMAKE_C_LINK_PIE_SUPPORTED)
  message(STATUS "Enabling PIE (Position Independent Executable) support")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
else()
  message(STATUS "PIE (Position Independent Executable) not enabled")
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

  # LTO
  if (LTO_SUPPORTED)
    set_target_properties(${TARGET_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
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

  if(NOT LIB_NO_LIBPREFIX AND NOT LIB_STATIC)
    if (WIN32)
       set_target_properties(${LIB_NAME} PROPERTIES PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}Telegram.Bot.")
    else()
       set_target_properties(${LIB_NAME} PROPERTIES PREFIX "${CMAKE_SHARED_LIBRARY_PREFIX}TgBot")
    endif()
  endif()
  set_target_properties(${LIB_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

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
                        "NAME;RELATION" "SRCS;LIBS;DEFS" ${ARGN})

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

  if (EXE_OPTIONAL)
    add_executable(${TARGET_NAME} EXCLUDE_FROM_ALL ${EXE_SRCS})
    add_dependencies(optional_all ${TARGET_NAME})
  else()
    add_executable(${TARGET_NAME} ${EXE_SRCS})
  endif()
  if(NOT EXE_NO_PREFIX)
    set_target_properties(${TARGET_NAME} PROPERTIES PREFIX "${PROJECT_NAME}_")
  endif()

  tgbot_common_target(${TARGET_NAME} PUBLIC_LIBS ${EXE_LIBS} DEFS ${EXE_DEFS})

  if (EXE_TEST)
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
if (TGBOTCPP_RUST_MODULES)
  add_subdirectory(src/third-party/corrosion)
  corrosion_import_crate(MANIFEST_PATH src/rust/stringcalc/Cargo.toml)
endif()
# ##############################################################################

# ############################ Check lib (TODO: Make these optional) #############################
find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(absl REQUIRED)
find_package(Protobuf)
find_package(fruit REQUIRED)
# ##############################################################################

if (fmt_VERSION VERSION_LESS 10.0.0)
   message(FATAL_ERROR "fmt version >= 10.0.0 is required")
endif()

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
  find_package(dlfcn-win32 REQUIRED)
  set(CMAKE_DL_LIBS dlfcn-win32::dl)
endif()

add_subdirectory(src/api)
add_subdirectory(src/database)
add_subdirectory(src/random)
add_subdirectory(src/imagep)
add_subdirectory(src/logging)
add_subdirectory(src/utils)
add_subdirectory(src/libos)
add_subdirectory(src/third-party/stduuid)
add_subdirectory(src/third-party/tgbot-cpp)
set(CRYPTOPP_BUILD_TESTING OFF CACHE BOOL "Disable cryptopp tests" FORCE)
add_subdirectory(src/third-party/cryptopp-cmake)

# Tests
if (TGBOTCPP_BUILD_TESTS)
  find_package(GTest REQUIRED)
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
  STATIC)

tgbot_library(NAME ThreadManager SRCS src/ManagedThread.cpp
              src/ThreadManager.cpp STATIC)
# ##############################################################################

# Optional httplib webserver
set(httplib_def)
set(httplib_srcs)
set(httplib_libs)
find_package(httplib)
if(httplib_FOUND)
  set(httplib_def TGBOTCPP_ENABLE_WEBSERVER)
  set(httplib_srcs src/web/TgBotWebServer.cpp)
  set(httplib_libs Web)
  add_subdirectory(src/web)
  install(
    DIRECTORY www/
    DESTINATION share/${PROJECT_NAME}/www
  )
  message(STATUS "Found httplib, enabling webserver support")
endif()

# ################ The Bot's main launcher (program) #################
tgbot_exe(
  NAME
  main
  SRCS
  src/main.cpp
  src/global_handlers/SpamBlocker.cpp
  src/global_handlers/SpamBlockManager.cpp
  src/ml/ChatDataCollector.cpp
  src/logging/LoggingServer.cpp
  ${httplib_srcs}
  DEFS
  ${httplib_def}
  LIBS
  DBLoading
  fruit
  Utils
  ${httplib_libs}
  Random
  restartfmt_parser
  Regex
  SocketService
  sighandler
  ApiImpl
  ThreadManager
  LogServiceProto
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
file(COPY www/ DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME}/www)

# Set CPack configurations
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Telegram bot coded in C++")
set(CPACK_PACKAGE_CONTACT "roynatech@gmail.com")
set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TgBot C++ server ${CPACK_PACKAGE_VERSION}")

if(WIN32)
  # NSIS
  list(APPEND CPACK_GENERATOR NSIS)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/resources/photo/icon.ico)
  set(CPACK_NSIS_MUI_UNIICON ${CMAKE_SOURCE_DIR}/resources/photo/uninstall.ico)
  set(CPACK_NSIS_DISPLAY_NAME "The Glider Project")
  set(CPACK_NSIS_WELCOME_TITLE "Welcome to The Glider Project Installer")
  set(CPACK_NSIS_BRANDING_TEXT "Created with CMake ${CMAKE_VERSION}")
  set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
  set(CPACK_NSIS_INSTALLED_ICON_NAME
      "${CMAKE_BINARY_DIR}/bin/${PROJECT_NAME}_main.exe")
  set(CPACK_NSIS_MANIFEST_DPI_AWARE TRUE)
endif()

if (LINUX)
  # DEB
  list(APPEND CPACK_GENERATOR DEB)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Soo Hwan Na <roynatech@gmail.com>")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
  set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
endif()

set(CPACK_PACKAGE_FILE_NAME 
    "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)

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
