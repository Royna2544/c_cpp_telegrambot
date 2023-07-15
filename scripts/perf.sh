#!/bin/bash

set -e

rm -f CMakeCache.txt
CC=clang CXX=clang++ CFLAGS="-flto=full -O3" CXXFLAGS="-flto=full -O3" cmake .
make -j$(nproc)
