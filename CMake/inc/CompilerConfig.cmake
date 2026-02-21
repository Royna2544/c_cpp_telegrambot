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
set(GLOBAL_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/src/include ${CMAKE_SOURCE_DIR}/src)

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

# Configure Ccache if available
find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  message(STATUS "Found ccache")
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
else()
  message(STATUS "Could NOT find ccache (this is NOT an error)")
endif()

add_library(GlobalCompilerSettings INTERFACE)
target_compile_definitions(GlobalCompilerSettings INTERFACE ${GLOBAL_DEFINITIONS})
target_compile_options(GlobalCompilerSettings INTERFACE ${GLOBAL_COMPILE_OPTIONS})
target_include_directories(GlobalCompilerSettings INTERFACE ${GLOBAL_INCLUDE_DIRS})