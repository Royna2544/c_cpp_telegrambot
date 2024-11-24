if (NOT LINUX OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    return()
endif()

find_package(libgit2 REQUIRED)

add_my_library(
    NAME BuilderUtilsLinux
    SRCS 
        RepoUtils.cpp
        ForkAndRun.cpp
        Shmem.cpp
    LIBS
        TgBotLogInit
        TgBotSocketSelectors
        TgBotsighandler
        ${LIBGIT2_LIBS}
    NO_LIBPREFIX
    STATIC
)

add_subdirectory(android)
add_subdirectory(kernel)