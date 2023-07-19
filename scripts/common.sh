#!/bin/bash

function common_cmake() {
    set -e
    # Clean CMake caches, if CMakeCache doesn't exist on PWD, just clean everything
    rm -f CMakeCache.txt
    CFLAGS=$FLAGS CXXFLAGS=$FLAGS cmake $(git rev-parse --show-toplevel)
    make -j$(nproc)
}
