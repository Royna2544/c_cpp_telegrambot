#include "libfs.hpp"

#include <ResourceManager.h>
#include <absl/log/log.h>

#include <filesystem>

#include "GitData.h"

std::filesystem::path FS::getPath(PathType type) {
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
            path =
                getPath(PathType::GIT_ROOT) / ResourceManager::kResourceDirname;
            ok = true;
            break;
        case PathType::RESOURCES_SQL:
            path = getPath(PathType::RESOURCES) / "sql";
            ok = true;
            break;
        case PathType::RESOURCES_WEBPAGE:
            path = getPath(PathType::GIT_ROOT) / "www";
            ok = true;
            break;
        case PathType::RESOURCES_SCRIPTS:
            path = getPath(PathType::RESOURCES) / "scripts";
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

constexpr FS::SharedLibType FS::SharedLib;       // NOLINT
constexpr std::string_view FS::kDylibExtension;  // NOLINT