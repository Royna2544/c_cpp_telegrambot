#pragma once

#include <string>

#ifdef __WIN32
static inline const char path_env_delimiter = ';';
#else
static inline const char path_env_delimiter = ':';
#endif

// Implemented sperately by OS
bool canExecute(const std::string& path);
bool getHomePath(std::string& buf);
bool fileExists(const std::string& filename);