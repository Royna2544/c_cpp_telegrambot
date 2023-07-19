#!/bin/bash

. $(dirname $0)/common.sh

export CC=gcc
export CXX=g++
export FLAGS=-g

common_cmake
