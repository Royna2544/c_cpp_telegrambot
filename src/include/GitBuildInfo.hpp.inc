#pragma once

#include <string_view>

namespace git::buildinfo {
constexpr static std::string_view COMMIT_ID = "@GIT_COMMIT_ID@";
constexpr static std::string_view COMMIT_MESSAGE = "@GIT_COMMIT_MESSAGE@";
constexpr static std::string_view ORIGIN_URL = "@GIT_ORIGIN_URL@";
}  // namespace git::buildinfo