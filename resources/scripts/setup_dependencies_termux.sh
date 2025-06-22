if [ ! -x "/data/data/com.termux/files/usr/bin/pkg" ]; then
    echo "Is not Termux. return"
    exit 1
fi

if [ "$1" == "all" ]; then
    EXTRA_PKGS="googletest libpng libjpeg-turbo libwebp"
fi

pkg install git boost boost-headers jsoncpp cmake fmt clang protobuf sqlite jsoncpp-static ninja libgit2 binutils-is-llvm flatbuffers flatbuffers-static $EXTRA_PKGS

if [ $? -ne 0 ]; then
    echo "Error: Failed to install required packages."
    exit 1
fi

git submodule update --init
