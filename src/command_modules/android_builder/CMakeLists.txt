find_package(jsoncpp)

if (NOT ${jsoncpp_FOUND})
    message(STATUS "libjsoncpp not found, disable AndroidBuilder-cpp")
    return()
endif()

################# Android ROM builder with C++ #################
if (UNIX AND NOT APPLE)
add_my_library(
    NAME AndroidBuilder-cpp
    SRCS 
        RepoUtils.cpp
        ForkAndRun.cpp
        ConfigParsers.cpp
        Shmem.cpp
        SystemInfo.cpp
    LIBS
        TgBotLogInit
        TgBotRandom
        TgBotSocketSelectors
        JsonCpp::JsonCpp
        ${LIBGIT2_LIBS}
    PUBLIC_INC
        .
    NO_LIBPREFIX
    STATIC
)
endif()