#include <cstdlib>
#include <cstring>
#include <random>
#include "libutils.h"

#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

#ifdef __WIN32
#include <windows.h>
#include <shlwapi.h>
#include <shlobj.h>
bool canExecute(const std::string& path) {
    auto filepath = path.c_str();
    bool exists = PathFileExistsA(filepath);
    if (!exists) {
        return false;
    }

    bool isDirectory = PathIsDirectoryA(filepath);
    if (isDirectory) {
        return false;
    }
    return true;
}
bool getHomePath(std::string& buf) {
    CHAR userDir[MAX_PATH];
    bool ret = SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, userDir));
    if (ret) {
        buf = userDir;
    }
    return ret;
}
#else
#include <unistd.h>
bool getHomePath(std::string& buf) {
    auto buf_c = getenv("HOME");
    if (buf_c) {
        buf = buf_c;
    }
    return !!buf_c;
}
bool canExecute(const std::string& path) {
    return access(path.c_str(), R_OK | X_OK) == 0;
}
#endif

#ifdef __linux__
#include <linux/limits.h> // I need PATH_MAX
#endif

std::vector<std::string> getPathEnv() {
    size_t pos = 0;
    std::vector<std::string> paths;
    std::string path;
    const char* path_c = getenv("PATH");
    if (!path_c) {
        return {};
    }
    path = path_c;
    while ((pos = path.find(path_env_delimiter)) != std::string::npos) {
        paths.emplace_back(path.substr(0, pos));
        path.erase(0, pos + 1);
    }
    return paths;
}

void findCompiler(char** c, char** cxx) {
    static const char* const compilers[][2] = {
        {"clang", "clang++"},
        {"gcc", "g++"},
        {"cc", "c++"},
    };
    static char buffer[PATH_MAX];
    for (const auto& path : getPathEnv()) {
        for (int i = 0; i < ARRAY_SIZE(compilers); i++) {
            auto checkfn = [i](const std::string& pathsuffix, const int idx) -> bool {
                memset(buffer, 0, sizeof(buffer));
                auto bytes = snprintf(buffer, sizeof(buffer), "%s%c%s",
                                    pathsuffix.c_str(), dir_delimiter, compilers[i][idx]);
                #ifdef __WIN32
                    bytes += sizeof(".exe");
                    if (bytes >= sizeof(buffer))
                        return false;
                    strcat(buffer, ".exe");
                #endif
                buffer[bytes] = '\0';
                return canExecute(buffer);
            };
            if (!*c && checkfn(path, 0)) {
                *c = strdup(buffer);
            }
            if (!*cxx && checkfn(path, 1)) {
                *cxx = strdup(buffer);
            }
            if (*c && *cxx) return;
        }
    }
    
}

int genRandomNumber(const int num1, const int num2) {
    std::random_device rd;
    std::mt19937 gen(rd());
    int num1_l = num1, num2_l = num2;
    if (num1 > num2) {
        num1_l = num2;
        num2_l = num1;
    }
    std::uniform_int_distribution<int> distribution(num1_l, num2_l);
    return distribution(gen);
}
