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
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

## Sanitizers configuration
set(SANITIZER_CONFIG "ASan")
set(SANITIZER_FLAG)

if (NOT SANITIZER_CONFIG)
    message(STATUS "No sanitizers enabled")
elseif (SANITIZER_CONFIG STREQUAL "ASan")
    message(STATUS "Address sanitizer enabled")
    if (NOT APPLE)
        set(SANITIZER_FLAG "-fsanitize=address,leak")
    else() # On apple, leak sanitizer is not implemented
        set(SANITIZER_FLAG "-fsanitize=address")
    endif()
elseif (SANITIZER_CONFIG STREQUAL "TSan")
    message(STATUS "Thread sanitizer enabled")
    set(SANITIZER_FLAG "-fsanitize=thread")
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

