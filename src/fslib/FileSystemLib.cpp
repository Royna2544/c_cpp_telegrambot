#include <filesystem>
#include <string>

#include "Logging.h"

namespace fs = std::filesystem;

__attribute__((weak))
bool fileExists(const std::string& filename) {
    bool rc;

    std::error_code ec;
    rc = fs::exists(filename, ec);
    if (ec) {
        LOG_W("FS::Exists failed: %s", ec.message().c_str());
    }
    return rc;
}

bool canExecute(const std::string& filename)
{
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