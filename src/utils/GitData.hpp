#pragma once

#include <UtilsExports.h>

#include <filesystem>
#include <string>

struct Utils_API GitData {
    std::string commitid, commitmsg, originurl;
    std::filesystem::path gitSrcRoot;
    static bool Fill(GitData *gitData);
    bool Fill();
};