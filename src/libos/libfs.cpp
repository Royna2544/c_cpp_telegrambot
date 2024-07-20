#include "libfs.hpp"

#include <absl/log/log.h>

#include <CommandLine.hpp>
#include <filesystem>

#include "GitData.h"

namespace fs = std::filesystem;

bool FS::canExecute(const std::filesystem::path& filename) {
    std::error_code ec;

    if (exists(filename)) {
        auto status = fs::status(filename, ec);
        auto permissions = status.permissions();

        return (permissions & fs::perms::owner_exec) != fs::perms::none ||
               (permissions & fs::perms::group_exec) != fs::perms::none ||
               (permissions & fs::perms::others_exec) != fs::perms::none;
    }

    return false;
}

std::filesystem::path& FS::appendDylibExtension(std::filesystem::path& path) {
    if (!path.has_extension()) {
        path += kDylibExtension;
    }

    return path;
}

std::filesystem::path& FS::appendExeExtension(std::filesystem::path& path) {
#ifdef WINDOWS_BUILD
    if (!path.has_extension()) {
        path += ".exe";
    }
#endif

    return path;
}

std::filesystem::path& FS::makeRelativeToCWD(std::filesystem::path& path) {
    if (path.is_absolute()) {
        path.make_preferred();
        path = path.lexically_relative(
            std::filesystem::current_path().make_preferred());
    }
    return path;
}

std::filesystem::path FS::getPathForType(PathType type) {
    std::filesystem::path path;
    GitData data;
    bool ok = false;

    switch (type) {
        case PathType::HOME:
            if (getHomePath(path)) ok = true;
            break;
        case PathType::GIT_ROOT:
            if (GitData::Fill(&data)) {
                path = data.gitSrcRoot;
                ok = true;
            } else {
                // If we cannot determine, use build_root/../
                path = getPathForType(PathType::BUILD_ROOT).parent_path();
                ok = true;
            }
            break;
        case PathType::RESOURCES:
            path = getPathForType(PathType::GIT_ROOT) / "resources";
            // ResourceManager::kResourceDirname;
            ok = true;
            break;
        case PathType::MODULES_INSTALLED:
            if (!GitData::Fill(&data)) {
                path = getPathForType(PathType::BUILD_ROOT).parent_path() / "lib" / "commands";
                ok = true;
                break;
            }
            [[fallthrough]];
        case PathType::BUILD_ROOT: {
            path = std::filesystem::path((*CommandLine::getInstance())[0])
                       .parent_path();
            ok = true;
            break;
        }
        case PathType::RESOURCES_SQL:
            path = getPathForType(PathType::RESOURCES) / "sql";
            ok = true;
            break;
        case PathType::RESOURCES_WEBPAGE:
            path = getPathForType(PathType::GIT_ROOT) / "www";
            ok = true;
            break;
    }
    if (ok) {
        path.make_preferred();
    } else {
        LOG(ERROR) << "Could not find path for type " << static_cast<int>(type);
    }
    return path;
}