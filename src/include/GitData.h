#pragma once

#include <filesystem>
#include <string>

struct GitData {
    std::string commitid, commitmsg, originurl;
    std::filesystem::path gitSrcRoot;
    static bool Fill(GitData *gitData);
};