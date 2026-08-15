set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_BUILD_TYPE release)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Protobuf's RepeatedField container annotations are emitted from both inline
# application code and the compiled runtime. Keep target libraries on the same
# sanitizer instrumentation as the application so parser allocations have
# valid ASan container state. Host tools use vcpkg's separate default triplet.
set(_GLIDER_SANITIZER_FLAGS
    "-fsanitize=address,undefined -fno-omit-frame-pointer")
set(VCPKG_C_FLAGS "${_GLIDER_SANITIZER_FLAGS}")
set(VCPKG_CXX_FLAGS "${_GLIDER_SANITIZER_FLAGS}")
set(VCPKG_LINKER_FLAGS "-fsanitize=address,undefined")
