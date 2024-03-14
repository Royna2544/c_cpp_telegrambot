#include "libfs.hpp"

#include <filesystem>
#include <string>

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

std::filesystem::path& appendDylibExtension(std::filesystem::path& path) {
    if (!path.has_extension()) {
#ifdef _WIN32
        path += ".dll";
#else
        path += ".so";
#endif
    }

    return path;
}

std::filesystem::path& appendExeExtension(std::filesystem::path& path) {
#ifdef _WIN32
    if (!path.has_extension()) {
        path += ".exe";
    }
#endif

    return path;
}

std::filesystem::path& makeRelativeToCWD(std::filesystem::path& path) {
    if (path.is_absolute()) {
        path.make_preferred();
        path = path.lexically_relative(
            std::filesystem::current_path().make_preferred());
    }
    return path;
}