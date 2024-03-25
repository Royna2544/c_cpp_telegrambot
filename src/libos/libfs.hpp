#pragma once

#include <filesystem>

#include "libfs.h"

// Implemented sperately by OS

struct FS {
#ifdef __WIN32
    constexpr static char path_env_delimiter = ';';
#else
    constexpr static char path_env_delimiter = ':';
#endif
    enum class PathType {
        HOME,
        GIT_ROOT,
        RESOURCES,
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

    static std::filesystem::path& appendDylibExtension(std::filesystem::path& path);
    static std::filesystem::path& appendExeExtension(std::filesystem::path& path);
    static std::filesystem::path& makeRelativeToCWD(std::filesystem::path& path);

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
