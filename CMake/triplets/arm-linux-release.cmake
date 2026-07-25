set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)

# Compiler flags live in the chainloaded toolchain. vcpkg deliberately does
# not apply VCPKG_C_FLAGS or VCPKG_CXX_FLAGS when a chainload toolchain is set.
