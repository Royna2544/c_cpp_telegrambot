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

include(CMake/inc/CompilerConfig.cmake)
include(CMake/inc/GitBuildInfo.cmake)
include(CMake/inc/TargetDeclare.cmake)
if (TGBOTCPP_RUST_MODULES)
  include(CMake/inc/RustModule.cmake)
endif()
include(CMake/inc/Dependencies.cmake)

# ############################ Platforms #############################
if(WIN32)
  set(TARGET_VARIANT windows)
else()
  set(TARGET_VARIANT posix)
endif()
# ##############################################################################

# ####################### Include subprojects ######################

# Tests
if (TGBOTCPP_BUILD_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()
  add_subdirectory(tests)
else()
  message(STATUS "Disabling tests")
endif()

# We create this for test suites.
add_library(Regex INTERFACE)
target_sources(Regex INTERFACE src/global_handlers/RegEXHandler.cpp)

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
  NAME main
  SRCS
    src/main.cpp
    src/global_handlers/SpamBlocker.cpp
    src/global_handlers/SpamBlockManager.cpp
    src/ml/ChatDataCollector.cpp
    src/logging/LoggingServer.cpp
    src/ThreadManager.cpp
    src/ManagedThread.cpp
    ${httplib_srcs}
  RELATION Main)

target_link_libraries(main PRIVATE 
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
  LogServiceProto
)
target_compile_definitions(main PRIVATE ${httplib_def})
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

include(CMake/inc/CPackDefs.cmake)
# Include last for CPack
add_subdirectory(src/api)
add_subdirectory(src/database)
add_subdirectory(src/random)
add_subdirectory(src/imagep)
add_subdirectory(src/logging)
add_subdirectory(src/utils)
add_subdirectory(src/libos)
add_subdirectory(src/command_modules)
