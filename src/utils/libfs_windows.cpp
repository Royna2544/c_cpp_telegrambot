#include <shlobj.h>
#include <shlwapi.h>
#include <filesystem>
#include <string_view>
#include "libfs.hpp"

bool FS::exists(const std::filesystem::path& path) {
    std::string filepath(path.string());
    return PathFileExistsA(filepath.c_str()) && !PathIsDirectoryA(filepath.c_str());
}

bool FS::getHomePath(std::filesystem::path& buf) {
    CHAR userDir[MAX_PATH];
    bool ret =
        SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, userDir));
    if (ret) {
        buf = userDir;
    }
    return ret;
}

bool FS::deleteFile(const std::filesystem::path &filename) {
    std::string filepath(filename.string());
    return DeleteFileA(filepath.c_str()) != 0;
}