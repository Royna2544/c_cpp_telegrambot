#pragma once

#include <TgBotUtilsExports.h>

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
};

// Create a directory at the specified path, but allows EEXIST
inline std::error_code createDirectory(const std::filesystem::path& path) {
    std::error_code ec;
    if (std::filesystem::create_directory(path, ec)) {
        return {};
    }
    if (ec == std::errc::file_exists) {
        ec.clear();
    }
    return ec;
}

template <int push = 0>
std::optional<std::filesystem::path> walk_up_tree(
    const std::function<bool(const std::filesystem::path&)>& predicate,
    const std::filesystem::path& starting = std::filesystem::current_path()) {
    auto current = starting;
    for (int x = 0; x < push; ++x) {
        current = current.parent_path();
    }
    while (current != starting.root_path()) {
        if (predicate(current)) {
            return current;
        }
        current = current.parent_path();
    }
    return std::nullopt;
}

template <int push = 0>
std::vector<std::filesystem::path> walk_up_tree_and_gather(
    const std::function<bool(const std::filesystem::path&)>& predicate,
    const std::filesystem::path& starting = std::filesystem::current_path()) {
    std::vector<std::filesystem::path> result;
    auto current = starting;
    for (int x = 0; x < push; ++x) {
        current = current.parent_path();
    }
    while (current != starting.root_path()) {
        if (predicate(current)) {
            result.emplace_back(current);
        }
        current = current.parent_path();
    }
    return result;
}

inline std::filesystem::path operator/(std::filesystem::path path, FS::SharedLibType  /*test*/) {
    if (!path.has_extension()) {
        path += FS::kDylibExtension;
    }
    return path;
}