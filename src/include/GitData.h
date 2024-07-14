#pragma once

#include <TgBotUtilsExports.h>

#include <filesystem>
#include <string>

struct TgBotUtils_API GitData {
    std::string commitid, commitmsg, originurl;
    std::filesystem::path gitSrcRoot;
    static bool Fill(GitData *gitData);
    bool Fill(void);
};