#include <ResourceManager.h>
#include <absl/log/log.h>
#include <absl/strings/ascii.h>

#include <fstream>
#include <libos/libfs.hpp>
#include <sstream>
#include <system_error>

bool ResourceManager::preloadOneFile(std::filesystem::path path) {
    std::ifstream file(path);
    std::stringstream linebuf;
    bool found = false;

    path.make_preferred();
    for (auto p = path.parent_path(); p != path.root_path();
         p = p.parent_path()) {
        if (p.filename() == kResourceDirname) {
            found = true;
            break;
        }
    }
    if (found) {
        path = path.lexically_relative(
            FS::getPathForType(FS::PathType::RESOURCES));
    }

    if (std::ranges::find(ignoredResources, path.string()) !=
        ignoredResources.end()) {
        DLOG(INFO) << "Ignoring resource path " << path;
        return false;
    }
    for (const auto& [elem, data] : kResources) {
        if (elem.string() == path.string()) {
            LOG(WARNING) << "Resource " << path << " already loaded";
            return false;
        }
    }
    DLOG(INFO) << "Preloading " << path;
    linebuf << file.rdbuf();
    kResources[path] = linebuf.str();
    return true;
}

void ResourceManager::preloadResourceDirectory() {
    auto rd = FS::getPathForType(FS::PathType::RESOURCES);
    std::ifstream ifs(rd / kResourceLoadIgnoreFile);
    std::error_code ec;

    if (ifs) {
        std::string line;
        while (std::getline(ifs, line)) {
            line = absl::StripAsciiWhitespace(line);
            if (!line.empty()) {
                ignoredResources.emplace_back(line);
            }
        }
        ignoredResources.emplace_back(kResourceLoadIgnoreFile);
        LOG(INFO) << "Applied resource ignore configuration";
    }

    LOG(INFO) << "Preloading resource directory: " << rd;
    for (const auto& it : std::filesystem::directory_iterator(rd, ec)) {
        if (it.is_regular_file()) {
            preloadOneFile(it);
        }
    }
    if (ec) {
        LOG(ERROR) << "Failed to preload resource directory: " << ec.message();
    } else {
        LOG(INFO) << "Preloading resource directory done";
    }
}

const std::string& ResourceManager::getResource(std::filesystem::path path) {
    path.make_preferred();
    for (const auto& [elem, data] : kResources) {
        if (elem.string() == path.string()) {
            return data;
        }
    }
    LOG(ERROR) << "Resource not found: " << path;
    throw std::runtime_error(std::string("Resource not found: ") +
                             path.string());
}

DECLARE_CLASS_INST(ResourceManager);