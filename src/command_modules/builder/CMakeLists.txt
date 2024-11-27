if (NOT LINUX OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    return()
endif()

find_package(libgit2 REQUIRED)
find_package(LibArchive REQUIRED)

add_my_library(
    NAME BuilderUtilsLinux
    SRCS 
        RepoUtils.cpp
        ForkAndRun.cpp
        Shmem.cpp
        SystemInfo.cpp
        archives/Zip.cpp
        archives/Tar.cpp
    LIBS
        TgBotLogInit
        TgBotSocketSelectors
        TgBotsighandler
        progress
    LIBS_STATIC
        ${LIBGIT2_LIBRARIES}
        LibArchive::LibArchive
    NO_LIBPREFIX
    STATIC
)

add_subdirectory(android)
add_subdirectory(kernel)