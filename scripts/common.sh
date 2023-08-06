#!/bin/bash

function common_cmake() {
    set -e
    rm -f CMakeCache.txt
    CFLAGS="$FLAGS $CFLAGS"
    CXXFLAGS="$FLAGS $CXXFLAGS" 
    LDFLAGS="$FLAGS $LDFLAGS"
    export CFLAGS CXXFLAGS LDFLAGS
    cmake $(git rev-parse --show-toplevel)
    make -j$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)
}
