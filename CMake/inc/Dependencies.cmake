
# Fmt and spdlog are required, but we try to find them first before fetching to avoid unnecessary fetches during development.
find_package(fmt)

if (NOT fmt_FOUND)
# -----------------------------
# Dependencies (FetchContent)
# -----------------------------
include(FetchContent)

set(FMT_TEST OFF CACHE BOOL "disabling fmt tests" FORCE)

# Important: Match the fmt version with spdlog's bundled version to avoid ODR issues.
FetchContent_Declare(
    fmt
    GIT_REPOSITORY  https://github.com/fmtlib/fmt.git
    GIT_TAG         12.1.0
    GIT_PROGRESS    TRUE
     USES_TERMINAL_DOWNLOAD TRUE
)
FetchContent_GetProperties(fmt)
if (NOT fmt_POPULATED)
    FetchContent_MakeAvailable(fmt)
endif()
else()
  message(STATUS "Found fmt ${fmt_VERSION}")
  if (fmt_VERSION VERSION_LESS 10.0.0)
    message(FATAL_ERROR "fmt version >= 10.0.0 is required"
                        " (found ${fmt_VERSION})")
  endif()
endif()

# Try to find nlohmann_json first to avoid unnecessary fetches during development.
find_package(nlohmann_json)
if (NOT nlohmann_json_FOUND)
  include(FetchContent)

  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_PROGRESS    TRUE
    USES_TERMINAL_DOWNLOAD TRUE
  )
  FetchContent_GetProperties(nlohmann_json)
  if (NOT nlohmann_json_POPULATED)
     FetchContent_MakeAvailable(nlohmann_json)
  endif()
else()
  message(STATUS "Found nlohmann_json ${nlohmann_json_VERSION}")
endif()

# Google - Fruit, Abseil
find_package(fruit REQUIRED)
find_package(absl REQUIRED)

# dlfcn (libdl)
if(WIN32)
  find_package(dlfcn-win32 REQUIRED)
  set(CMAKE_DL_LIBS dlfcn-win32::dl)
endif()

# Add bundled dependencies
add_subdirectory(src/third-party/stduuid)
add_subdirectory(src/third-party/tgbot-cpp)
set(CRYPTOPP_BUILD_TESTING OFF CACHE BOOL "Disable cryptopp tests" FORCE)
add_subdirectory(src/third-party/cryptopp-cmake)