find_package(libgit2 REQUIRED)

include(CheckStructHasMember)
check_struct_has_member(git_fetch_options depth git2/clone.h LIBGIT2_HAS_CLONE_DEPTH)
if (LIBGIT2_HAS_CLONE_DEPTH)
    set(BUILDER_UTILS_LINUX_DEFS LIBGIT2_HAS_CLONE_DEPTH)
endif()

add_my_library(
    NAME RepoSwitcher
    SRCS RepoUtils.cpp
    DEFS ${BUILDER_UTILS_LINUX_DEFS}
    LIBS_STATIC ${LIBGIT2_LIBRARIES}
    LIBS sighandler
    STATIC
    NO_LIBPREFIX
)

add_my_executable(
    NAME RepoSwitcherTest
    SRCS RepoSwitcherTest.cpp
    LIBS RepoSwitcher
    OPTIONAL
)

if (NOT LINUX OR NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    message(STATUS "Builder: Unsupported platform (Which is ${CMAKE_SYSTEM_PROCESSOR} on ${CMAKE_SYSTEM_NAME})")
    return()
endif()

find_package(LibArchive)
find_package(Boost CONFIG)

if (NOT LibArchive_FOUND OR NOT Boost_FOUND)
    message(STATUS "Builder: No libarchive or Boost.Unit")
    return()
endif()

add_my_library(
    NAME BuilderUtilsLinux
    SRCS 
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
        LibArchive::LibArchive
        Boost::boost
        RepoSwitcher
    NO_LIBPREFIX
    STATIC
)

add_subdirectory(android)
add_subdirectory(kernel)
