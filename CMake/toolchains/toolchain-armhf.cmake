set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

# Debian and Ubuntu install the ARMHF target headers and libraries under this
# multiarch root. CMake's find modes below deliberately prohibit host paths,
# so expose the target root explicitly as well as relying on GCC's built-in
# include search. This is required by vcpkg ports that use find_file(), such
# as gettext-libintl's libintl.h probe.
list(PREPEND CMAKE_FIND_ROOT_PATH "/usr/arm-linux-gnueabihf")
list(REMOVE_DUPLICATES CMAKE_FIND_ROOT_PATH)

# This file is chainloaded by vcpkg, so the triplet's VCPKG_C_FLAGS and
# VCPKG_CXX_FLAGS are intentionally not applied. Define the target flags here
# for C, C++, and assembler sources. Keep vcpkg Linux's normal -fPIC because
# static dependencies are linked into Glider's shared command modules. The
# explicit FPU is required when ports such as Crypto++ add a bare
# -march=armv7-a to individual assembly files.
set(_TGBOTCPP_ARMHF_FLAGS "-fPIC -march=armv7-a -mfpu=neon -mfloat-abi=hard")
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
