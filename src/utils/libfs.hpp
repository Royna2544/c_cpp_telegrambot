#pragma once

#include <UtilsExports.h>
#include <absl/log/log.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <system_error>

namespace FS {

struct SharedLibType {};
static constexpr SharedLibType SharedLib{};

static constexpr std::string_view kDylibExtension =
#ifdef _WIN32
    ".dll";
#elif defined __APPLE__
    ".dylib";
#else
    ".so";
#endif

}  // namespace FS

inline std::filesystem::path operator/(std::filesystem::path path,
                                       FS::SharedLibType /*test*/) {
    if (!path.has_extension()) {
        path += FS::kDylibExtension;
    }
    return path;
}

namespace FS {

/**
 * Returns the home directory path of the current user.
 *
 * The home directory is the directory where the current user stores their
 * personal files and configuration data.
 *
 * @param buf A reference to a std::filesystem::path object that will be set
 * to the home directory path.
 * @return `true` if the home directory was successfully retrieved, or
 * `false` if an error occurred.
 */
bool getHomePath(std::filesystem::path& buf);
};  // namespace FS

namespace noex_fs {

// Create a directory at the specified path
// But allows EEXIST
inline bool create_directories(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::create_directories(path, ec)) {
        DLOG(INFO) << "create_directories: " << path << ": ok";
        return true;
    }
    if (ec == std::errc::file_exists) {
        DLOG(INFO) << "create_directories: " << path << ": Already exists";
        return true;
    }
    LOG(ERROR) << "create_directories: " << path << ": fail, " << ec.message();
    return false;
}

#define DECLARE_FS_SHIM(name)                                               \
    inline bool name(const std::filesystem::path& path) {                   \
        std::error_code ec;                                                 \
        bool ret = std::filesystem::name(path, ec);                         \
        if (ec) {                                                           \
            LOG(ERROR) << #name ": " << path << ": fail, " << ec.message(); \
            return false;                                                   \
        }                                                                   \
        return ret;                                                         \
    }

DECLARE_FS_SHIM(exists);
DECLARE_FS_SHIM(remove);
DECLARE_FS_SHIM(remove_all);

}  // namespace noex_fs
