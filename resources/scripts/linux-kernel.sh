#!/bin/bash

APT_COMMAND="apt update; apt install git bc build-essential flex bison libssl-dev libelf-dev dwarves"

function lecho () {
    echo "$0: $1"
}

trap 'lecho "Exiting script with $?"' EXIT

if [ -x /usr/bin/apt ]; then
    if [ $UID -eq 0 ]; then
        lecho "Current user is root"
        eval $APT_COMMAND
    else
        lecho "Current user is not root, trying sudo"
        eval "sudo -u root bash -c \"$APT_COMMAND\"" || lecho "Failed to get root"
    fi
else
    lecho "Only Debian-based are supported for now (apt)"
    exit 1;
fi
