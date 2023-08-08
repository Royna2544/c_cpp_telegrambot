#include <shlobj.h>
#include <shlwapi.h>
#include <windows.h>

#include <string>

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