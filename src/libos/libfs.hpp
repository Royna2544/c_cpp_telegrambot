#include <filesystem>
#include "libfs.h"

#ifdef __WIN32
static inline const char path_env_delimiter = ';';
#else
static inline const char path_env_delimiter = ':';
#endif

// Implemented sperately by OS

/**
 * Returns the home directory path of the current user.
 *
 * The home directory is the directory where the current user stores their personal
 * files and configuration data.
 *
 * @param buf A reference to a std::filesystem::path object that will be set to
 * the home directory path.
 * @return `true` if the home directory was successfully retrieved, or `false` if
 * an error occurred.
 */
bool getHomePath(std::filesystem::path& buf);

/**
 * Returns whether the current user can execute the specified file.
 *
 * @param path The path to the file.
 * @return `true` if the current user can execute the file, or `false` if not.
 */
bool canExecute(const std::filesystem::path& path);

/**
 * Checks if a file exists.
 *
 * @param filename the path to the file
 * @return true if the file exists, false otherwise
 */
bool fileExists(const std::filesystem::path& filename);