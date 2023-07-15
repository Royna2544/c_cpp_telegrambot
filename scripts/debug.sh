#!/bin/bash

set -e

rm -f CMakeCache.txt
CFLAGS=-g CXXFLAGS=-g cmake .
make -j$(nproc)
