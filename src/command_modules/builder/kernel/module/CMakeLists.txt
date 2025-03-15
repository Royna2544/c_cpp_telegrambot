command_module(
    NAME kernelbuild
    SRCS kernelbuild.cpp
    LIBS KernelBuilder-cpp Utils
    PLATFORM "Linux"
)
