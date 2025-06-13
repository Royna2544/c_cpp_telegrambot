cmake_minimum_required(VERSION 3.14)
project(TgBot++ LANGUAGES CXX C)

# CMake 3.25 added LINUX variable
if (CMAKE_VERSION VERSION_LESS "3.25")
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LINUX TRUE CACHE BOOL "Linux platform")
  endif()
endif()

####################### Set CMake Policy #######################
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

############################## OPTIONS ##############################
option(ENABLE_RUST_SUPPORT "Enable rust support" OFF)
option(SANITIZER_CONFIG "Sanitizers config for debug builds (ASan, TSan, etc)" "ASan")
option(BUILD_SHARED_LIBS "Build some dependent libraries as shared" OFF)
option(TGBOT_ENABLE_TESTING "Enable testing" OFF)
#####################################################################

#####################################################################
# Make vcpkg copy the dlls required.
set(VCPKG_APPLOCAL_DEPS ON CACHE BOOL "Copy VCPKG's dlls to exe directory" FORCE)
# On installation too.
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON CACHE BOOL "Copy VCPKG's dlls to install" FORCE)
#####################################################################

######################### C++ Configuration #########################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
#####################################################################

## Check PIE support
include(CheckPIESupported)
check_pie_supported()
if (CMAKE_CXX_LINK_PIE_SUPPORTED AND CMAKE_C_LINK_PIE_SUPPORTED)
  message(STATUS "Enabling PIE (Position Independent Executable) support")
  set(PIE_SUPPORTED TRUE CACHE BOOL "Whether PIE is supported")
else()
  message(STATUS "PIE (Position Independent Executable) not enabled")
endif()

## Sanitizers configuration
set(SANITIZER_FLAG)

if (NOT SANITIZER_CONFIG)
    message(STATUS "No sanitizers enabled")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Disabling sanitizers as release build")
# MSYS2 is special - They don't have any sanitizers
elseif (MINGW OR MSYS)
    message(STATUS "MSYS2 detected - Disabling sanitizers")
elseif (SANITIZER_CONFIG STREQUAL "ASan")
    message(STATUS "Address sanitizer enabled")
    if (APPLE) # On apple, leak sanitizer is not implemented
        set(SANITIZER_FLAG "-fsanitize=address")
    elseif (MSVC)
        set(SANITIZER_FLAG "/fsanitize=address")
    else()
        set(SANITIZER_FLAG "-fsanitize=address,leak")
    endif()
elseif (SANITIZER_CONFIG STREQUAL "UBSan")
    if (MSVC)
        set(SANITIZER_FLAG "/fsanitize=undefined")
    else()
        set(SANITIZER_FLAG "-fsanitize=undefined")
    endif()
elseif (SANITIZER_CONFIG STREQUAL "TSan")
    message(STATUS "Thread sanitizer enabled")
    if (MSVC)
        set(SANITIZER_FLAG "/fsanitize=thread")
    else()
        set(SANITIZER_FLAG "-fsanitize=thread")
    endif()
elseif (SANITIZER_CONFIG STREQUAL "MSan")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(STATUS "Memory sanitizer enabled")
        set(SANITIZER_FLAG "-fsanitize=memory")
    else()
        message(WARNING "Memory sanitizer is only supported with Clang")
    endif()
else()
    message(WARNING "Unknown sanitizer configuration: ${SANITIZER_CONFIG}")
endif()

# Define a macro to add sanitizers to a target.
function(add_sanitizers target)
    if (SANITIZER_FLAG)
        target_compile_options(${target} PRIVATE ${SANITIZER_FLAG})
        target_link_options(${target} PRIVATE ${SANITIZER_FLAG})
    endif()
endfunction()

## Compiler specific settings
set(GLOBAL_COMPILE_OPTIONS)
set(GLOBAL_DEFINITIONS)
set(GLOBAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR}/src/)

# Test proper c++20 jthread, stop_token
set(_jthread_test_source "
#include <thread>
#include <stop_token>

int main() {
   std::stop_token st;
   std::jthread jt;
}
")

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("${_jthread_test_source}" CXX20_JTHREAD_COMPILES)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  # Clang may opt in to experimental library
  if (NOT CXX20_JTHREAD_COMPILES)
    set(CMAKE_REQUIRED_FLAGS "-fexperimental-library")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
    check_cxx_source_compiles("${_jthread_test_source}" CXX20_JTHREAD_COMPILES_EXPERIMENTAL)
    if (CXX20_JTHREAD_COMPILES_EXPERIMENTAL)
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

if (NOT CXX20_JTHREAD_COMPILES)
  message(FATAL_ERROR "Sorry, your compiler is too old")
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT LTO_SUPPORTED)
  if (LTO_SUPPORTED)
    message(STATUS "Enabling LTO (Link time optimization)")
  endif()
endif()
if (WIN32)
  list(APPEND GLOBAL_DEFINITIONS _WIN32_WINNT=0x0A00)
  if (MSVC)
    list(APPEND GLOBAL_COMPILE_OPTIONS /utf-8)
    list(APPEND GLOBAL_DEFINITIONS NOMINMAX)
  endif()
endif()
if (NOT MSVC)
  list(APPEND GLOBAL_COMPILE_OPTIONS -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)
  list(APPEND GLOBAL_COMPILE_OPTIONS -Wno-unused-parameter)
endif()

# Build Type
list(APPEND GLOBAL_DEFINITIONS BUILD_TYPE_STR="${CMAKE_BUILD_TYPE}")

##################### Fill in readme with cmake #####################
string(TIMESTAMP TODAY "%Y-%m-%d %H:%M:%S UTC" UTC)

find_package(Git REQUIRED) # It will obviously have
function(git_execute_proc)
  cmake_parse_arguments(GIT_PROC
    "" # Option
    "NAME;VAR" # Single
    "COMMAND" # Multiple
    ${ARGN}
  )
  if (NOT (GIT_PROC_NAME AND GIT_PROC_VAR AND GIT_PROC_COMMAND))
    message(SEND_ERROR "Missing arguments")
  endif()
  
  # Run the git command to get the commit ID or other info
  execute_process(
    COMMAND ${GIT_EXECUTABLE} ${GIT_PROC_COMMAND}
    OUTPUT_VARIABLE GIT_PROC_OUT
    ERROR_VARIABLE GIT_PROC_ERR
    RESULT_VARIABLE GIT_PROC_RESULT
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )

  # Check if the git command was successful
  if (GIT_PROC_RESULT EQUAL 0)
    set(${GIT_PROC_VAR} ${GIT_PROC_OUT} PARENT_SCOPE)
    message(STATUS "Git ${GIT_PROC_NAME}: ${GIT_PROC_OUT}")
  else()
    set(${GIT_PROC_VAR} "-" PARENT_SCOPE)
    message(WARNING "Error retrieving Git ${GIT_PROC_NAME}: ${GIT_PROC_ERR}")
  endif()
endfunction()

git_execute_proc(
  COMMAND rev-parse HEAD
  NAME "commit-id"
  VAR GIT_COMMIT_ID
)

git_execute_proc(
  COMMAND log -1 --pretty=%B
  NAME "commit-message"
  VAR GIT_COMMIT_MESSAGE
)

git_execute_proc(
  COMMAND remote get-url origin
  NAME "origin-url"
  VAR GIT_ORIGIN_URL
)

configure_file(resources/about.html.in ${CMAKE_SOURCE_DIR}/resources/about.html)
configure_file(src/include/GitBuildInfo.hpp.inc ${CMAKE_SOURCE_DIR}/src/include/GitBuildInfo.hpp)
#####################################################################

################## Declare common macros ####################
function(tgbot_library)
  cmake_parse_arguments(TGBOT_LIBRARY
    "NO_LIBPREFIX;STATIC" # Options
    "NAME" # One-value keywords
    "SRCS;SRCS_UNIX;LIBS;LIBS_STATIC;PUBLIC_INC;PRIVATE_INC;DEPENDS;DEFS;LIBS_WIN32" # Multiple-value keywords
    ${ARGN}
  )
  if (NOT TGBOT_LIBRARY_SRCS OR NOT TGBOT_LIBRARY_NAME)
    message(FATAL_ERROR "Must specify src or name")
  endif()
  if (UNIX)
    list(APPEND TGBOT_LIBRARY_SRCS ${TGBOT_LIBRARY_SRCS_UNIX})
  endif()
  if (TGBOT_LIBRARY_STATIC)
    add_library(${TGBOT_LIBRARY_NAME} STATIC ${TGBOT_LIBRARY_SRCS})
  else()
    add_library(${TGBOT_LIBRARY_NAME} SHARED ${TGBOT_LIBRARY_SRCS})
    if (WIN32)
      install(TARGETS ${TGBOT_LIBRARY_NAME} DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    else()
      install(TARGETS ${TGBOT_LIBRARY_NAME} DESTINATION lib)
    endif()
  endif()
  if (NOT TGBOT_LIBRARY_NO_LIBPREFIX)
    set_target_properties(${TGBOT_LIBRARY_NAME} PROPERTIES PREFIX ${PROJECT_NAME}_)
  endif()
  add_sanitizers(${TGBOT_LIBRARY_NAME})
  
  set(EXPORTCONF_DIR ${CMAKE_BINARY_DIR}/exportConfig/)
  if (NOT TGBOT_LIBRARY_STATIC)
    configure_file(${CMAKE_SOURCE_DIR}/src/include/LibraryExports.h.inc
      ${EXPORTCONF_DIR}${TGBOT_LIBRARY_NAME}Exports.h)
  endif()
  target_include_directories(${TGBOT_LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${TGBOT_LIBRARY_PUBLIC_INC} ${EXPORTCONF_DIR})
  target_include_directories(${TGBOT_LIBRARY_NAME} PRIVATE ${TGBOT_LIBRARY_PRIVATE_INC})
  list(APPEND TGBOT_LIBRARY_LIBS absl::log absl::check fmt::fmt)
  if (TGBOT_LIBRARY_STATIC)
    foreach(n ${TGBOT_LIBRARY_LIBS})
      target_include_directories(${TGBOT_LIBRARY_NAME} PRIVATE $<TARGET_PROPERTY:${n},INTERFACE_INCLUDE_DIRECTORIES>)
      target_compile_definitions(${TGBOT_LIBRARY_NAME} PRIVATE $<TARGET_PROPERTY:${n},INTERFACE_COMPILE_DEFINITIONS>)
    endforeach()
    target_link_libraries(${TGBOT_LIBRARY_NAME} INTERFACE ${TGBOT_LIBRARY_LIBS})
  else()
    target_link_libraries(${TGBOT_LIBRARY_NAME} PUBLIC ${TGBOT_LIBRARY_LIBS})
  endif()
  target_link_libraries(${TGBOT_LIBRARY_NAME} PRIVATE ${TGBOT_LIBRARY_LIBS_STATIC})
  if (WIN32)
    target_link_libraries(${TGBOT_LIBRARY_NAME} PUBLIC ${TGBOT_LIBRARY_LIBS_WIN32})
  endif()
  if (TGBOT_LIBRARY_DEPENDS)
    add_dependencies(${TGBOT_LIBRARY_NAME} ${TGBOT_LIBRARY_DEPENDS})
  endif()
  target_compile_definitions(${TGBOT_LIBRARY_NAME} PUBLIC ${TGBOT_LIBRARY_DEFS})

  # Set library output directory
  if (WIN32)
    set_target_properties(${TGBOT_LIBRARY_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  else()
    set_target_properties(${TGBOT_LIBRARY_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  endif()

  # PIE
  if (PIE_SUPPORTED)
    set_target_properties(${TGBOT_LIBRARY_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  if (LTO_SUPPORTED)
    set_target_properties(${TGBOT_LIBRARY_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
  # Global variables
  target_compile_definitions(${TGBOT_LIBRARY_NAME} PRIVATE ${GLOBAL_DEFINITIONS})
  target_compile_options(${TGBOT_LIBRARY_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
  target_include_directories(${TGBOT_LIBRARY_NAME} PRIVATE ${GLOBAL_INCLUDE_DIRS})
endfunction(tgbot_library)

function(tgbot_exe)
  cmake_parse_arguments(TGBOT_EXE
    "NO_PREFIX;OPTIONAL;TEST" # Options
    "NAME;RELATION" # One-value keywords
    "SRCS;LIBS" # Multiple-value keywords
    ${ARGN}
  )
  if (NOT TGBOT_EXE_SRCS OR NOT TGBOT_EXE_NAME)
    message(FATAL_ERROR "Must specify src or name")
  endif()
  set(TGBOT_EXE_NAME_BAK ${TGBOT_EXE_NAME})
  if (TGBOT_EXE_TEST)
    set(TEST_EXT test_)
  endif()
	set(TGBOT_EXE_NAME ${TEST_EXT}${TGBOT_EXE_NAME})
  list(APPEND TGBOT_EXE_SRCS ${CMAKE_SOURCE_DIR}/src/log_message.cpp)
  if (TGBOT_EXE_OPTIONAL)
    add_executable(${TGBOT_EXE_NAME} EXCLUDE_FROM_ALL ${TGBOT_EXE_SRCS})
  else()
    add_executable(${TGBOT_EXE_NAME} ${TGBOT_EXE_SRCS})
    if (TGBOT_EXE_TEST)
      add_test(NAME TestSuite_${TGBOT_EXE_NAME_BAK} COMMAND ${TGBOT_EXE_NAME})
      if (MSVC AND BUILD_SHARED_LIBS)
        # https://github.com/google/googletest/issues/292
        target_compile_definitions(${TGBOT_EXE_NAME} PRIVATE GTEST_LINKED_AS_SHARED_LIBRARY)
      endif()
    else()
      if (NOT TGBOT_EXE_RELATION)
        message(SEND_ERROR "Please specify relation: ${TGBOT_EXE_NAME} Valid are: Socket,Database,Main")
      endif()
      install(TARGETS ${TGBOT_EXE_NAME} COMPONENT App${TGBOT_EXE_RELATION})
    endif()
  endif()
  if (NOT TGBOT_EXE_NO_PREFIX)
    set_target_properties(${TGBOT_EXE_NAME} PROPERTIES PREFIX ${PROJECT_NAME}_)
  endif()
  add_sanitizers(${TGBOT_EXE_NAME})
  # Set RPATH for Unix-like systems
  if (UNIX)
    # bin/main bin/cli lib/libTgBot.so
    set_target_properties(${TGBOT_EXE_NAME} PROPERTIES INSTALL_RPATH_USE_LINK_PATH TRUE)
    set_target_properties(${TGBOT_EXE_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
  endif()
  target_link_libraries(${TGBOT_EXE_NAME} LogInit absl::log ${TGBOT_EXE_LIBS})
  if (WIN32)
     set_target_properties(${lib} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  else()
     set_target_properties(${lib} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  endif()
  if (WIN32)
    add_custom_command(TARGET ${TGBOT_EXE_NAME} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy -t $<TARGET_FILE_DIR:${TGBOT_EXE_NAME}> $<TARGET_RUNTIME_DLLS:${TGBOT_EXE_NAME}>
      COMMAND_EXPAND_LISTS
    )
    install(FILES $<TARGET_RUNTIME_DLLS:${TGBOT_EXE_NAME}> TYPE BIN)
  endif()

  # Set library output directory
  if (NOT TGBOT_EXE_TEST)
    set_target_properties(${TGBOT_EXE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  endif()
  # LTO
  if (LTO_SUPPORTED)
    set_target_properties(${TGBOT_LIBRARY_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
  # PIE
  if (PIE_SUPPORTED)
    set_target_properties(${TGBOT_EXE_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  # Global variables
  target_compile_definitions(${TGBOT_EXE_NAME} PRIVATE ${GLOBAL_DEFINITIONS})
  target_compile_options(${TGBOT_EXE_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
  target_include_directories(${TGBOT_EXE_NAME} PRIVATE ${GLOBAL_INCLUDE_DIRS})
endfunction(tgbot_exe)

############################ RUST support ###########################
if (ENABLE_RUST_SUPPORT)
  add_subdirectory(src/third-party/corrosion)
  corrosion_import_crate(MANIFEST_PATH src/rust/stringcalc/Cargo.toml)
endif()
#####################################################################

############################# Check lib #############################
find_package(Protobuf)
find_package(fmt REQUIRED)
find_package(jsoncpp REQUIRED) # Required by tgbot-cpp anyways...
find_package(CURL REQUIRED)
# If we couldn't find protobuf or dirty abseil fix is enabled.
if (NOT Protobuf_FOUND)
  if (MSVC)
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
#####################################################################

# Configure Ccache if available
find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
  message(STATUS "Found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
else()
  message(STATUS "Could NOT find ccache (this is NOT an error)")
endif()

############################# Platforms #############################
if(WIN32)
  set(TARGET_VARIANT windows)
else()
  set(TARGET_VARIANT posix)
endif()
#####################################################################

######################## Include subprojects ######################
# dlfcn (libdl)
if (WIN32)
  add_subdirectory(src/third-party/dlfcn-win32)
  set(CMAKE_DL_LIBS dl)
elseif(ANDROID)
  set(CMAKE_DL_LIBS)
else()
  find_library(DL dl)
  add_library(dl SHARED IMPORTED)
  set_target_properties(dl PROPERTIES
    IMPORTED_LOCATION "${DL}"
  )
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
find_package(Python3 QUIET)
if (Python3_FOUND)
  message(STATUS "Enabling cpp-httplib compilation")
  set(HTTPLIB_COMPILE ON)
endif()
add_subdirectory(src/third-party/cpp-httplib)

# Tests
if (TGBOT_ENABLE_TESTING)
  if (MSVC)
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
################### The Bot's main functionaility ###################
tgbot_library(
  NAME Regex
  SRCS
    src/global_handlers/RegEXHandler.cpp
  LIBS absl::status
  STATIC
)

tgbot_library(
  NAME SocketComponents
  SRCS
    src/global_handlers/SpamBlocker.cpp
    src/global_handlers/ChatObserver.cpp
    src/socket/bot/SocketDataHandler.cpp
    src/socket/bot/SocketInterface.cpp
  LIBS TgBot DBImpl Socket
  STATIC
)

tgbot_library(
  NAME ThreadManager
  SRCS
    src/ManagedThread.cpp
    src/ThreadManager.cpp
  STATIC
)
#####################################################################

################# The Bot's main launcher (program) #################

tgbot_exe(
  NAME main
  SRCS
    src/main.cpp
    src/global_handlers/SpamBlockManager.cpp
    src/ml/ChatDataCollector.cpp
    src/web/TgBotWebServer.cpp
    src/logging/LoggingServer.cpp
  LIBS DBLoading fruit TgBot Utils Web Random restartfmt_parser Regex
       SocketComponents sighandler Api ApiComponents ThreadManager
  RELATION Main
)

# Add an icon
if (WIN32)
    target_sources(main PRIVATE resources/photo/resource.rc)
endif()

if (UNIX)
tgbot_exe(
  NAME maind
  SRCS src/mainDaemon.cpp
  RELATION Main
)
endif()

#####################################################################
# Resource dirs
install(DIRECTORY resources/ DESTINATION share/${PROJECT_NAME} FILES_MATCHING PATTERN "*"
  PATTERN "*.in" EXCLUDE)
file(COPY resources/ DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME})

# Set CPack configurations
set(CPACK_PACKAGE_NAME "TgBot++")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Telegram bot coded in C++")
set(CPACK_PACKAGE_CONTACT "support@example.com")
set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TgBot C++ server ${CPACK_PACKAGE_VERSION}")

if (WIN32)
  # NSIS
  list(APPEND CPACK_GENERATOR NSIS)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/resources/photo/icon.ico)
  set(CPACK_NSIS_MUI_UNIICON ${CMAKE_SOURCE_DIR}/resources/photo/uninstall.ico)
  set(CPACK_NSIS_DISPLAY_NAME "Telegram Bot Server")
  set(CPACK_NSIS_WELCOME_TITLE "Welcome! Actually, no")
  set(CPACK_NSIS_BRANDING_TEXT "Created with CMake ${CMAKE_VERSION}")
  set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
  set(CPACK_NSIS_INSTALLED_ICON_NAME "${CMAKE_BINARY_DIR}/bin/${PROJECT_NAME}_main.exe")
  set(CPACK_NSIS_MANIFEST_DPI_AWARE TRUE)
endif()

# Include CPack
include(CPack)

cpack_add_component_group(OptionalCli
    DISPLAY_NAME "Optional Cli tools"
    DESCRIPTION "Additional CLIs."
    EXPANDED # Expand the group by default in the installer UI
)

cpack_add_component(AppMain
    DISPLAY_NAME "Main Application"
    DESCRIPTION "The main server application."
    REQUIRED # Ensures this component is always installed
)

cpack_add_component(AppSocket
    DISPLAY_NAME "Socket Utils"
    DESCRIPTION "Socket connection clients."
    GROUP "OptionalCli"
)

cpack_add_component(AppDatabase
    DISPLAY_NAME "Database Utils"
    DESCRIPTION "Database manipulation utils."
    GROUP "OptionalCli"
)

# Include last for CPack
add_subdirectory(src/command_modules)
