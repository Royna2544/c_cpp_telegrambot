set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)
set(
    VCPKG_CHAINLOAD_TOOLCHAIN_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../toolchains/toolchain-armhf.cmake"
)

# Compiler flags live in the chainloaded toolchain above. vcpkg deliberately
# does not apply VCPKG_C_FLAGS or VCPKG_CXX_FLAGS when one is set.
