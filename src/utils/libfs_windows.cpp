#include <shlobj.h>
#include <shlwapi.h>
#include <filesystem>
#include <string_view>
#include "libfs.hpp"

bool FS::getHomePath(std::filesystem::path& buf) {
    CHAR userDir[MAX_PATH];
    bool ret =
        SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, userDir));
    if (ret) {
        buf = userDir;
    }
    return ret;
}
