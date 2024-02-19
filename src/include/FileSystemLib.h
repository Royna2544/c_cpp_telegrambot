#pragma once

#include <filesystem>

#ifdef __WIN32
static inline const char path_env_delimiter = ';';
#else
static inline const char path_env_delimiter = ':';
#endif

// Implemented sperately by OS
bool canExecute(const std::filesystem::path& path);
bool getHomePath(std::filesystem::path& buf);
bool fileExists(const std::filesystem::path& filename);