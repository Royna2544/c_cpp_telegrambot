#pragma once

#include <string_view>

namespace buildinfo {

namespace git {
constexpr static std::string_view COMMIT_ID = "@GIT_COMMIT_ID@";
constexpr static std::string_view COMMIT_MESSAGE = R"(@GIT_COMMIT_MESSAGE@)";
constexpr static std::string_view ORIGIN_URL = "@GIT_ORIGIN_URL@";
}  // namespace git

constexpr std::string_view OS =
#if defined(_WIN32)
    "Windows";
#elif defined(__ANDROID__)
    "Android";
#elif defined(__linux__)
    "Linux";
#elif defined(__APPLE__)
    "macOS";
#elif defined(__FreeBSD__)
    "FreeBSD";
#else
    "unknown platform";
#endif

enum class BuildType { DEBUG, RELEASE };

#ifdef NDEBUG
constexpr BuildType kBuildType = BuildType::RELEASE;
#else
constexpr BuildType kBuildType = BuildType::DEBUG;
#endif

inline constexpr bool isReleaseBuild() noexcept {
    return kBuildType == BuildType::RELEASE;
}
inline constexpr bool isDebugBuild() noexcept {
    return kBuildType == BuildType::DEBUG;
}

}  // namespace buildinfo
