#include <filesystem>
#include <string>

namespace fs = std::filesystem;

bool canExecute(const std::string& filename)
{
    std::error_code ec;

    if (fs::exists(filename, ec)) {
        auto status = fs::status(filename, ec);
        auto permissions = status.permissions();

        return (permissions & fs::perms::owner_exec) != fs::perms::none ||
            (permissions & fs::perms::group_exec) != fs::perms::none ||
            (permissions & fs::perms::others_exec) != fs::perms::none;
    }

    return false;
}