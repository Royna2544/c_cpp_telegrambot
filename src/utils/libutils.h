#pragma once

#ifdef __cplusplus
#include <string>
#include <vector>

enum ProgrammingLangs {
    C,
    CXX,
    GO,
    PYTHON,
};

std::string findCommandExe(const std::string& basename);
std::string findCompiler(ProgrammingLangs lang);

// libbase
bool ReadFdToString(int fd, std::string* content);
bool ReadFileToString(const std::string& path, std::string* content);

// Compile version
std::string getCompileVersion();
int genRandomNumber(const int num1, const int num2 = 0);

// Path
#ifdef __WIN32
static inline const char path_env_delimiter = ';';
static inline const char dir_delimiter = '\\';
#else
static inline const char path_env_delimiter = ':';
static inline const char dir_delimiter = '/';
#endif

// Path tools
std::vector<std::string> getPathEnv();
// below functions are implemented sperately by OS
bool canExecute(const std::string& path);
bool getHomePath(std::string& buf);

// Src path
std::string getSrcRoot();
static inline std::string getResourcePath(const std::string& filename) {
    return getSrcRoot() + "/resources/" + filename;
}

// Command
bool runCommand(const std::string& command, std::string& res);
#endif

#ifndef TEMP_FAILURE_RETRY
/* Used to retry syscalls that can return EINTR. */
#define TEMP_FAILURE_RETRY(exp) ({         \
    __typeof__(exp) _rc;                   \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc; })
#endif

#include "config.h"
#define IS_DEFINED IS_BUILTIN
#define PRETTYF(fmt, ...) printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
