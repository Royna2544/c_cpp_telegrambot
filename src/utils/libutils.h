#pragma once

#include <string>
#include <vector>

enum ProgrammingLangs {
    C,
    CXX,
    GO,
    PYTHON,
};

std::string findCommandExe(std::string command);
std::string findCompiler(ProgrammingLangs lang);

// Compile version
std::string getCompileVersion();

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

// Command
bool runCommand(const std::string& command, std::string& res);

// MIME
std::string getMIMEString(const std::string& path);
