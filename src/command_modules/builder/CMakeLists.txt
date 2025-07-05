find_package(libgit2)
if(NOT LIBGIT2_FOUND)
  message(STATUS "Builder: No libgit2")
  return()
endif()

include(CheckStructHasMember)
check_struct_has_member(git_fetch_options depth git2/clone.h
                        LIBGIT2_HAS_CLONE_DEPTH)
if(LIBGIT2_HAS_CLONE_DEPTH)
  set(BUILDER_UTILS_LINUX_DEFS LIBGIT2_HAS_CLONE_DEPTH)
endif()

tgbot_library(
  NAME
  RepoSwitcher
  SRCS
  RepoUtils.cpp
  DEFS
  ${BUILDER_UTILS_LINUX_DEFS}
  LIBS
  ${LIBGIT2_LIBRARIES}
  STATIC
  NO_LIBPREFIX)

tgbot_exe(
  NAME
  RepoSwitcherTest
  SRCS
  RepoSwitcherTest.cpp
  LIBS
  RepoSwitcher
  OPTIONAL)

find_package(LibArchive)
if(WIN32)
  find_package(Boost CONFIG COMPONENTS units)
else()
  find_package(Boost CONFIG)
endif()
find_package(CURL)

if(NOT CURL_FOUND OR NOT Boost_FOUND)
  message(STATUS "Builder: No cURL or Boost.Units")
  return()
endif()

tgbot_library(
  NAME
  CurlDownload
  SRCS
  CurlUtils.cpp
  LIBS
  CURL::libcurl
  Boost::headers
  sighandler
  STATIC)

if(NOT LibArchive_FOUND)
  message(STATUS "Builder: No libarchive")
  return()
endif()

add_subdirectory(archives)

if(NOT LINUX AND NOT ANDROID)
  message(STATUS "Builder: Unsupported platform: ${CMAKE_SYSTEM_NAME})")
  return()
endif()

tgbot_library(
  NAME
  BuilderUtilsLinux
  SRCS
  ForkAndRun.cpp
  Shmem.cpp
  SystemInfo.cpp
  PollWrap.cpp
  LIBS
  sighandler
  progress
  Boost::headers
  RepoSwitcher
  NO_LIBPREFIX
  STATIC)

add_subdirectory(android)
add_subdirectory(kernel)
