if (NOT LINUX OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    message(STATUS "Builder: Unsupported platform (Which is ${CMAKE_SYSTEM_PROCESSOR} on ${CMAKE_SYSTEM_NAME})")
    return()
endif()

find_package(libgit2 REQUIRED)
find_package(LibArchive)

if (NOT LibArchive_FOUND)
    message(STATUS "Builder: No libarchive")
    return()
endif()

include(CheckStructHasMember)
check_struct_has_member(git_fetch_options depth git2/clone.h LIBGIT2_HAS_CLONE_DEPTH)

if (LIBGIT2_HAS_CLONE_DEPTH)
    set(BUILDER_UTILS_LINUX_DEFS LIBGIT2_HAS_CLONE_DEPTH)
endif()

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
        LogInit
        SocketSelectors
        sighandler
        progress
    LIBS_STATIC
        ${LIBGIT2_LIBRARIES}
        LibArchive::LibArchive
    DEFS ${BUILDER_UTILS_LINUX_DEFS}
    NO_LIBPREFIX
    STATIC
)

add_subdirectory(android)
add_subdirectory(kernel)