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

std::string findCommandExe(const std::string& command);
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

#define LOG_F(fmt, ...) _LOG(fmt, "FATAL", ##__VA_ARGS__)
#define LOG_E(fmt, ...) _LOG(fmt, "Error", ##__VA_ARGS__)
#define LOG_W(fmt, ...) _LOG(fmt, "Warning", ##__VA_ARGS__)
#define LOG_I(fmt, ...) _LOG(fmt, "Info", ##__VA_ARGS__)
#ifndef NDEBUG
#define LOG_D(fmt, ...) _LOG(fmt, "Debug", ##__VA_ARGS__)
#else
#define LOG_D(fmt, ...) do {} while(0)
#endif

#define _LOG(fmt, servere, ...) printf("[%s:%d] " servere ": " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
