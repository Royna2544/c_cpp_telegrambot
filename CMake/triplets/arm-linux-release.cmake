set(VCPKG_TARGET_ARCHITECTURE arm)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_BUILD_TYPE release)

# The arm-linux-gnueabihf toolchain defaults to the hard-float ABI, whose
# default -march (armv7-a+fp) already provides an FPU. Some ports (e.g.
# Crypto++'s cryptogams *_armv4.S assembly) override the arch with a bare
# -march=armv7-a, which drops the FPU and makes the hard-float ABI fail with
# "selected architecture lacks an FPU". Pin an explicit NEON FPU so an FPU is
# always available regardless of per-source -march overrides. (The .S files are
# driven through the C compiler, so VCPKG_C_FLAGS is what actually fixes them.)
set(VCPKG_C_FLAGS "-mfpu=neon")
set(VCPKG_CXX_FLAGS "-mfpu=neon")
set(VCPKG_ASM_FLAGS "-mfpu=neon")
