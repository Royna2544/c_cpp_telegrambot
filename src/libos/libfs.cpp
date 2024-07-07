#include "libfs.hpp"

#include <absl/log/log.h>

#include <filesystem>

#include "ConfigManager.h"
#include "GitData.h"
#include "ResourceManager.h"

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
#ifdef WINDOWS_BUILD
        path += ".dll";
#elif defined __APPLE__
        path += ".dylib";
#else
        path += ".so";
#endif
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
            }
            break;
        case PathType::RESOURCES:
            path = getPathForType(PathType::GIT_ROOT) /
                   ResourceManager::kResourceDirname;
            ok = true;
            break;
        case PathType::MODULES_INSTALLED:
        case PathType::BUILD_ROOT: {
            int argc = 0;
            char* const* argv = nullptr;
            copyCommandLine(CommandLineOp::GET, &argc, &argv);
            if (argv != nullptr) {
                path = std::filesystem::path(argv[0]).parent_path();
                ok = true;
            }
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