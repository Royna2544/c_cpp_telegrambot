find_package(libgit2 REQUIRED)

include(CheckStructHasMember)
check_struct_has_member(git_fetch_options depth git2/clone.h LIBGIT2_HAS_CLONE_DEPTH)
if (LIBGIT2_HAS_CLONE_DEPTH)
    set(BUILDER_UTILS_LINUX_DEFS LIBGIT2_HAS_CLONE_DEPTH)
endif()

tgbot_library(
    NAME RepoSwitcher
    SRCS RepoUtils.cpp
    DEFS ${BUILDER_UTILS_LINUX_DEFS}
    LIBS_STATIC ${LIBGIT2_LIBRARIES}
    LIBS sighandler
    STATIC
    NO_LIBPREFIX
)

tgbot_exe(
    NAME RepoSwitcherTest
    SRCS RepoSwitcherTest.cpp
    LIBS RepoSwitcher
    OPTIONAL
)

find_package(CURL REQUIRED)

tgbot_library(
    NAME CurlDownload
    SRCS CurlUtils.cpp
    LIBS CURL::libcurl
    STATIC
)

find_package(LibArchive)
find_package(Boost CONFIG)

if (NOT LibArchive_FOUND OR NOT Boost_FOUND)
    message(STATUS "Builder: No libarchive or Boost.Unit")
    return()
endif()

add_subdirectory(archives)

if (NOT LINUX AND NOT ANDROID)
    message(STATUS "Builder: Unsupported platform: ${CMAKE_SYSTEM_NAME})")
    return()
endif()

tgbot_library(
    NAME BuilderUtilsLinux
    SRCS 
        ForkAndRun.cpp
        Shmem.cpp
        SystemInfo.cpp
    LIBS
        LogInit
        SocketSelectors
        sighandler
        progress
    LIBS_STATIC
        Boost::boost
        RepoSwitcher
    NO_LIBPREFIX
    STATIC
)

add_subdirectory(android)
add_subdirectory(kernel)
