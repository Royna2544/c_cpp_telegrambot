command_module(
    NAME rombuild
    SRCS romBuild.cpp
    LIBS AndroidBuilder-cpp TgBotsighandler
    PLATFORM "Linux"
)