export CROSS_TOOLCHAIN_PREFIX=arm-linux-gnueabihf-
export CARGO_TARGET_ARMV7_UNKNOWN_LINUX_GNUEABIHF_LINKER="$CROSS_TOOLCHAIN_PREFIX"gcc \
    AR_armv7_unknown_linux_gnueabihf="$CROSS_TOOLCHAIN_PREFIX"ar \
    CC_armv7_unknown_linux_gnueabihf="$CROSS_TOOLCHAIN_PREFIX"gcc \
    CXX_armv7_unknown_linux_gnueabihf="$CROSS_TOOLCHAIN_PREFIX"g++ \
    RUST_TEST_THREADS=1 \
    PKG_CONFIG_PATH="/usr/lib/arm-linux-gnueabihf/pkgconfig/:${PKG_CONFIG_PATH}" \
    PKG_CONFIG_ALLOW_CROSS=1 \
    CROSS_CMAKE_SYSTEM_NAME=Linux \
    CROSS_CMAKE_SYSTEM_PROCESSOR=arm \
    CROSS_CMAKE_CRT=gnu \
    CROSS_CMAKE_OBJECT_FLAGS="-ffunction-sections -fdata-sections -fPIC -march=armv7-a -mfpu=neon -mfloat-abi=hard" \
    CROSS_CMAKE_SHARED_LINKER_FLAGS="-Wl,--gc-sections -Wl,-z,now -Wl,-z,relro" \
    CROSS_CMAKE_STATIC_LINKER_FLAGS="-Wl,--gc-sections -Wl,-z,now -Wl,-z,relro"
