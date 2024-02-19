#include <shlobj.h>
#include <shlwapi.h>

#include <filesystem>

bool getHomePath(std::filesystem::path& buf) {
    CHAR userDir[MAX_PATH];
    bool ret = SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, userDir));
    if (ret) {
        buf = userDir;
    }
    return ret;
}

bool fileExists(const std::filesystem::path& path) {
    auto pathstr = path.string();
    auto filepath = pathstr.c_str();
    return PathFileExistsA(filepath) && !PathIsDirectoryA(filepath);
}