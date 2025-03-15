command_module(
    NAME rombuild
    SRCS romBuild.cpp
    LIBS AndroidBuilder-cpp sighandler Archives Utils CurlDownload
    PLATFORM "Linux"
)
