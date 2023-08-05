#pragma once

#ifdef __cplusplus
#include <string>
#include <vector>
#endif

void findCompiler(char** c, char** cxx);

#ifndef __cplusplus
int genRandomNumber(const int num1, const int num2);
#else
int genRandomNumber(const int num1, const int num2 = 0);
std::vector<std::string> getPathEnv();
bool canExecute(const std::string& path);
bool getHomePath(std::string& buf);

#ifdef __WIN32
static inline const char path_env_delimiter = ';';
static inline const char dir_delimiter = '\\';
#else
static inline const char path_env_delimiter = ':';
static inline const char dir_delimiter = '/';
#endif
#endif

#define PRETTYF(fmt, ...) printf("[%s:%d] " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
