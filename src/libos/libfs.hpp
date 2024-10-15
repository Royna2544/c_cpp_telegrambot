#pragma once

#include <TgBotUtilsExports.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <system_error>

// Implemented sperately by OS

struct TgBotUtils_API FS {
#ifdef WINDOWS_BUILD
    constexpr static char path_env_delimiter = ';';
#else
    constexpr static char path_env_delimiter = ':';
#endif
    enum class PathType {
        HOME,
        GIT_ROOT,
        RESOURCES,
        RESOURCES_SQL,
        RESOURCES_WEBPAGE,
        BUILD_ROOT,
        MODULES_INSTALLED,
    };

    /**
     * Returns the path associated with the specified type.
     *
     * @param type The type of path to retrieve.
     * @return The path, if it exists, or an empty path.
     */
    static std::filesystem::path getPathForType(PathType type);

    /**
     * Returns whether the current user can execute the specified file.
     *
     * @param path The path to the file.
     * @return `true` if the current user can execute the file, or `false` if
     * not.
     */
    static bool canExecute(const std::filesystem::path& path);

    /**
     * Checks if a file exists.
     *
     * @param filename the path to the file
     * @return true if the file exists, false otherwise
     */
    static bool exists(const std::filesystem::path& filename);

    /**
     * Deletes the specified file.
     *
     * This function attempts to delete the file located at the given path.
     * If the file does not exist or cannot be deleted for any reason, this
     * function will return false.
     *
     * @param filename The path to the file to be deleted.
     * @return true if the file was successfully deleted, false otherwise.
     */
    static bool deleteFile(const std::filesystem::path& filename);

    static std::filesystem::path& appendDylibExtension(
        std::filesystem::path& path);
    static std::filesystem::path& appendExeExtension(
        std::filesystem::path& path);
    static std::filesystem::path& makeRelativeToCWD(
        std::filesystem::path& path);

    static constexpr std::string_view kDylibExtension =
#ifdef WINDOWS_BUILD
        ".dll";
#elif defined __APPLE__
        ".dylib";
#else
        ".so";
#endif

   private:
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
    static bool getHomePath(std::filesystem::path& buf);
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