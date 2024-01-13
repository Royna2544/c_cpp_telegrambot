#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>

#include <string>

bool canExecute(const std::string& path) {
    auto filepath = path.c_str();
    return PathFileExistsA(filepath) && !PathIsDirectoryA(filepath);
}

bool getHomePath(std::string& buf) {
    CHAR userDir[MAX_PATH];
    bool ret = SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, userDir));
    if (ret) {
        buf = userDir;
    }
    return ret;
}