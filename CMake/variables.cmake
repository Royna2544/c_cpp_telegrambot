#################### Submodule's configuration ###################
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build libs as shared libraries" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Build tests" FORCE)
set(ABSL_BUILD_TEST_HELPERS OFF CACHE BOOL "[abseil] Build gtest helpers" FORCE)
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "[abseil] Enable installation" FORCE)
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
set(CMAKE_EXE_LINKER_FLAGS "-fuse-ld=lld")
