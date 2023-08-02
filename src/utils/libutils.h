#pragma once

#include <vector>

void findCompiler(char** c, char** cxx);
int genRandomNumber(const int num1, const int num2 = 0);
std::vector<std::string> getPathEnv();
bool canExecute(const std::string& path);

#ifdef __WIN32
    static inline const char path_env_delimiter = ';';
    static inline const char dir_delimiter = '/';
#else
    static inline const char path_env_delimiter = ':';
    static inline const char dir_delimiter = '\\';
#endif

#define PRETTYF(fmt, ...) printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
