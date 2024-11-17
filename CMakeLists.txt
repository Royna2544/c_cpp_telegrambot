cmake_minimum_required(VERSION 3.25)
project(TgBot++ LANGUAGES CXX C)

####################### Set CMake Policy #######################
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# Set RPATH for Unix-like systems
if(UNIX)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()

############################## OPTIONS ##############################
option(ENABLE_RUST_SUPPORT "Enable rust support" OFF)
option(DISABLE_SANITIZERS "Disable all sanitizers (ASan, TSan, etc)" OFF)
option(DIRTY_ABSEIL_FIX "Enable dirty abseil linkage fix" OFF)
#####################################################################

#################### Submodule's configuration ###################
set(BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(ABSL_BUILD_TEST_HELPERS OFF CACHE BOOL "[abseil] Build gtest helpers" FORCE)
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "[abseil] Enable installation" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
#####################################################################
set(LIBS_INSTALL_PATH ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${LIBS_INSTALL_PATH} CACHE STRING 
  "Output to store binaries (win32)" FORCE)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIBS_INSTALL_PATH} CACHE STRING
  "Output to store binaries (linux)" FORCE)
#####################################################################

######################### C++ Configuration #########################
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
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
set(SANITIZER_CONFIG "ASan")
set(SANITIZER_FLAG)

if (NOT SANITIZER_CONFIG)
    message(STATUS "No sanitizers enabled")
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

if (DISABLE_SANITIZERS)
    message(STATUS "Sanitizers disabled by option")
    unset(SANITIZER_FLAG)
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Disabling sanitizers as release build")
    unset(SANITIZER_FLAG)
endif()
# MSYS2 is special - They don't have any sanitizers
if (MINGW OR MSYS)
    message(STATUS "MSYS2 detected - Disabling sanitizers")
    unset(SANITIZER_FLAG)
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
set(GLOBAL_DEFINITIONS -D__TGBOT__)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
  set(CMAKE_SHARED_LINKER_FLAGS "-fuse-ld=lld")
  if (APPLE OR ANDROID OR BSD)
    list(APPEND GLOBAL_COMPILE_OPTIONS -fexperimental-library)
  endif()
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT ANDROID)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT LTO_SUPPORTED)
  if (LTO_SUPPORTED)
    message(STATUS "Enabling LTO (Link time optimization)")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
  endif()
endif()
if (WIN32)
  list(APPEND GLOBAL_DEFINITIONS _WIN32_WINNT=0x0A00)
  if (MSVC)
    list(APPEND GLOBAL_COMPILE_OPTIONS /utf-8)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT SANITIZER_FLAG)
      list(APPEND GLOBAL_COMPILE_OPTIONS /ZI)
    endif()
  endif()
endif()
if (NOT MSVC)
  list(APPEND GLOBAL_COMPILE_OPTIONS -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)
endif()

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

##################### Fill in readme with cmake #####################
string(TIMESTAMP TODAY "%Y-%m-%d")
configure_file(resources/about.html.in ${CMAKE_SOURCE_DIR}/resources/about.html)
#####################################################################

################## Declare common macros ####################
function(add_my_library)
  cmake_parse_arguments(TGBOT_LIBRARY
    "NO_LIBPREFIX;STATIC" # Options
    "NAME" # One-value keywords
    "SRCS;LIBS;LIBS_STATIC;PUBLIC_INC;PRIVATE_INC;DEPENDS;DEFS;LIBS_WIN32" # Multiple-value keywords
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

  # PIE
  if (PIE_SUPPORTED)
    set_target_properties(${TGBOT_LIBRARY_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  # Global variables
  target_compile_definitions(${TGBOT_LIBRARY_NAME} PRIVATE ${GLOBAL_DEFINITIONS})
  target_compile_options(${TGBOT_LIBRARY_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
endfunction(add_my_library)

function(add_my_executable)
  cmake_parse_arguments(TGBOT_EXE
    "NO_PREFIX;OPTIONAL" # Options
    "NAME" # One-value keywords
    "SRCS;LIBS;LIBS_WIN32" # Multiple-value keywords
    ${ARGN}
  )
  if (NOT TGBOT_EXE_SRCS OR NOT TGBOT_EXE_NAME)
    message(FATAL_ERROR "Must specify src or name")
  endif()
  if (DIRTY_ABSEIL_FIX)
    list(APPEND TGBOT_EXE_SRCS ${CMAKE_SOURCE_DIR}/src/logging/log_message.cc)
  endif()
  if (NOT TGBOT_EXE_NO_PREFIX)
    set(TGBOT_EXE_NAME ${PROJECT_NAME}_${TGBOT_EXE_NAME})
  endif()
  if (TGBOT_EXE_OPTIONAL)
    add_executable(${TGBOT_EXE_NAME} EXCLUDE_FROM_ALL ${TGBOT_EXE_SRCS})
  else()
    add_executable(${TGBOT_EXE_NAME} ${TGBOT_EXE_SRCS})
    install(TARGETS ${TGBOT_EXE_NAME} DESTINATION bin)
  endif()
  add_sanitizers(${TGBOT_EXE_NAME})
  target_link_libraries(${TGBOT_EXE_NAME} TgBotLogInit absl::log ${TGBOT_EXE_LIBS})
  if (WIN32)
    target_link_libraries(${TGBOT_EXE_NAME} ${TGBOT_EXE_LIBS_WIN32})
  endif()

  # PIE
  if (PIE_SUPPORTED)
    set_target_properties(${TGBOT_EXE_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
  # Global variables
  target_compile_definitions(${TGBOT_EXE_NAME} PRIVATE ${GLOBAL_DEFINITIONS})
  target_compile_options(${TGBOT_EXE_NAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
endfunction(add_my_executable)

############################ RUST support ###########################
if (ENABLE_RUST_SUPPORT)
  add_subdirectory(src/third-party/corrosion)
  corrosion_import_crate(MANIFEST_PATH src/rust/stringcalc/Cargo.toml)
endif()
#####################################################################

######################## Include directories ########################
include_directories(src/include)
include_directories(src/)
#####################################################################

############################# Check lib #############################
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Protobuf)
find_package(fmt REQUIRED)
find_package(jsoncpp REQUIRED) # Required by tgbot-cpp anyways...
# If we couldn't find protobuf or dirty abseil fix is enabled.
if (DIRTY_ABSEIL_FIX OR NOT Protobuf_FOUND)
  # Then we can just include source-abseil
  message(STATUS "Using abseil-cpp as submodule")
  set(ABSL_PROPAGATE_CXX_STD ON)
  add_subdirectory(src/third-party/abseil-cpp)
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

######################## Define SQLite3 target ######################
add_subdirectory(src/database)
add_subdirectory(src/random)
add_subdirectory(src/command_modules)
add_subdirectory(src/imagep)
add_subdirectory(src/stringres)
add_subdirectory(src/logging)
add_subdirectory(src/socket)
add_subdirectory(src/utils)
add_subdirectory(src/web)
add_subdirectory(src/libos)
find_package(Python3 QUIET)
if (Python3_FOUND)
  message(STATUS "Enabling cpp-httplib compilation")
  set(HTTPLIB_COMPILE ON)
endif()
add_subdirectory(src/third-party/cpp-httplib)
add_subdirectory(src/third-party/tgbot-cpp)
add_subdirectory(src/third-party/fruit EXCLUDE_FROM_ALL)
add_subdirectory(tests)
#################################################################

add_my_library(
  NAME PPImpl_shared_deps
  SRCS
    src/api/MessageExt.cpp
  PUBLIC_INC
    $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>
  STATIC
)

################### The Bot's main functionaility ###################
add_my_library(
  NAME PPImpl
  SRCS 
    src/ManagedThread.cpp
    src/ThreadManager.cpp
    src/Authorization.cpp
    src/api/TgBotApiImpl.cpp
    src/api/CommandModule.cpp
    src/api/components/OnAnyMessage.cpp
    src/api/components/ChatJoinRequest.cpp
    src/api/components/OnCallbackQuery.cpp
    src/api/components/OnInlineQuery.cpp
    src/api/components/OnMyChatMember.cpp
    src/api/components/FileCheck.cpp
    src/api/components/Async.cpp
    src/api/components/Restart.cpp
    src/api/components/ModuleManagement.cpp
    src/api/components/UnknownCommand.cpp
    src/global_handlers/RegEXHandler.cpp
    src/global_handlers/SpamBlocker.cpp
    src/global_handlers/ChatObserver.cpp
    src/ml/ChatDataCollector.cpp
    src/web/TgBotWebServer.cpp
    src/logging/LoggingServer.cpp
    src/socket/interface/impl/bot/SocketDataHandler.cpp
    src/socket/interface/impl/bot/TgBotSocketInterface.cpp
    src/socket/interface/impl/backends/ServerBackend.cpp
    src/socket/interface/impl/backends/ServerBackend_${TARGET_VARIANT}.cpp
  LIBS  TgBot TgBotUtils TgBotWeb TgBotDBImpl absl::status TgBotRandom TgBot_restartfmt_parser
        TgBotPPImpl_shared_deps TgBotStringRes ${CMAKE_DL_LIBS} TgBotSocket JsonCpp::JsonCpp
  STATIC
)
#####################################################################

################# The Bot's main launcher (program) #################
add_my_executable(
  NAME main
  SRCS src/main.cpp
  LIBS TgBotPPImpl TgBotDBLoading TgBotsighandler TgBot_restartfmt_parser fruit
)

if (UNIX)
add_my_executable(
  NAME maind
  SRCS src/mainDaemon.cpp
)
endif()
#####################################################################

install(DIRECTORY resources/ DESTINATION resources FILES_MATCHING  PATTERN "*"
  PATTERN "*.in" EXCLUDE)
