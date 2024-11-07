find_package(jsoncpp)
find_package(libgit2 REQUIRED)
find_package(LibXml2 REQUIRED)

if (NOT ${jsoncpp_FOUND})
    message(STATUS "libjsoncpp not found, disable AndroidBuilder-cpp")
    return()
endif()

################# Android ROM builder with C++ #################
if (LINUX)
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
        progress
        JsonCpp::JsonCpp
        LibXml2::LibXml2
        TgBotsighandler
        ${LIBGIT2_LIBS}
    NO_LIBPREFIX
    STATIC
)

add_my_executable(
    NAME ConfigParserTest
    SRCS ConfigParserTest.cpp
    LIBS AndroidBuilder-cpp
    OPTIONAL
)

endif()