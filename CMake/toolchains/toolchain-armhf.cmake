set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# This file is chainloaded by vcpkg, so the triplet's VCPKG_C_FLAGS and
# VCPKG_CXX_FLAGS are intentionally not applied. Define the target flags here
# for C, C++, and assembler sources. The explicit FPU is required when ports
# such as Crypto++ add a bare -march=armv7-a to individual assembly files.
set(_TGBOTCPP_ARMHF_FLAGS "-march=armv7-a -mfpu=neon -mfloat-abi=hard")
string(APPEND CMAKE_C_FLAGS_INIT " ${_TGBOTCPP_ARMHF_FLAGS}")
string(APPEND CMAKE_CXX_FLAGS_INIT " ${_TGBOTCPP_ARMHF_FLAGS}")
string(APPEND CMAKE_ASM_FLAGS_INIT " ${_TGBOTCPP_ARMHF_FLAGS}")

# The all-important sandbox rules
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Cpack
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE armhf)
