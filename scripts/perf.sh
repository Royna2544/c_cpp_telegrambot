#!/bin/bash

. $(dirname $0)/common.sh

export CC=clang
export CXX=clang++
export FLAGS="-flto=full -O3"

common_cmake
