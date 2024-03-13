#include <filesystem>
#include <string>
#include "libfs.hpp"
#include "Logging.h"

namespace fs = std::filesystem;

bool canExecute(const std::filesystem::path& filename) {
    std::error_code ec;

    if (fileExists(filename)) {
        auto status = fs::status(filename, ec);
        auto permissions = status.permissions();

        return (permissions & fs::perms::owner_exec) != fs::perms::none ||
               (permissions & fs::perms::group_exec) != fs::perms::none ||
               (permissions & fs::perms::others_exec) != fs::perms::none;
    }

    return false;
}