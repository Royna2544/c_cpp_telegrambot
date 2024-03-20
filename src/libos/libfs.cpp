#include "libfs.hpp"

#include <filesystem>

#include "ConfigManager.h"
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
#ifdef _WIN32
        path += ".dll";
#else
        path += ".so";
#endif
    }

    return path;
}

std::filesystem::path& FS::appendExeExtension(std::filesystem::path& path) {
#ifdef _WIN32
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

std::optional<std::filesystem::path> FS::getPathForType(PathType type) {
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
            if (auto rootdir = getPathForType(PathType::GIT_ROOT);
                rootdir.has_value()) {
                path = rootdir.value() / "resources";
                ok = true;
            }
            break;
        case PathType::BUILD_ROOT: {
            int argc = 0;
            char* const* argv = nullptr;
            copyCommandLine(CommandLineOp::GET, &argc, &argv);
            if (argv != nullptr) {
                path = std::filesystem::path(argv[0]).parent_path();
                ok = true;
            }
        }
    }
    if (ok) {
        path.make_preferred();
        makeRelativeToCWD(path);
        return path;
    }
    return std::nullopt;
}