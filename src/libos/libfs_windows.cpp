#include <shlobj.h>
#include <shlwapi.h>

#include <filesystem>
#include <CStringLifetime.h>
#include "libfs.hpp"

bool FS::exists(const std::filesystem::path& path) {
    CStringLifetime filepath(path);
    return PathFileExistsA(filepath) && !PathIsDirectoryA(filepath);
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
    CStringLifetime filepath(filename);
    return DeleteFileA(filepath) != 0;
}