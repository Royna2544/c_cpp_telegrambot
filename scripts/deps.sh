#!/bin/bash

APT_COMMAND="apt update; apt install libcurl4-openssl-dev libssl-dev libboost-all-dev \
    cmake clang libprotobuf-dev protobuf-compiler ninja-build"

function lecho () {
    echo "$0: $1"
}

trap 'lecho "Exiting script with $?"' EXIT

if [ ! -x /usr/bin/apt ]; then
    lecho "Only Debian-based are supported for now (apt)"
    exit 1;
fi

if [ $UID -eq 0 ]; then
    lecho "Current user is root"
    eval $APT_COMMAND
else
    lecho "Current user is not root, trying sudo"
    eval "sudo -u root bash -c \"$APT_COMMAND\"" || lecho "Failed to get root"
fi

git submodule update --init
