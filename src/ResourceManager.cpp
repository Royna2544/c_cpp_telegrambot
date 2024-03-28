#include <Logging.h>
#include <ResourceManager.h>

#include <fstream>
#include <libos/libfs.hpp>
#include <mutex>
#include <sstream>

#include "StringToolsExt.h"

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
    if (found)
        path = path.lexically_relative(
            FS::getPathForType(FS::PathType::RESOURCES));

    std::call_once(m_once, [this] {
        std::ifstream ifs(FS::getPathForType(FS::PathType::RESOURCES) /
                          kResourceLoadIgnoreFile);
        if (ifs) {
            std::string line;
            while (std::getline(ifs, line)) {
                TrimStr(line);
                if (line.empty()) continue;
                ignoredResources.emplace_back(line);
            }
            ignoredResources.emplace_back(kResourceLoadIgnoreFile);
            LOG(LogLevel::INFO, "Applied resource ignore configuration");
        }
    });
    if (std::find(ignoredResources.begin(), ignoredResources.end(), path) !=
        ignoredResources.end()) {
        LOG(LogLevel::DEBUG, "Ignoring resource path %s",
            path.string().c_str());
        return false;
    }
    for (const auto& [elem, data] : kResources) {
        if (elem.string() == path.string()) {
            LOG(LogLevel::WARNING, "Resource %s already loaded",
                path.string().c_str());
            return false;
        }
    }
    LOG(LogLevel::VERBOSE, "Preloading %s", path.string().c_str());
    linebuf << file.rdbuf();
    kResources[path] = linebuf.str();
    return true;
}

void ResourceManager::preloadResourceDirectory() {
    std::filesystem::directory_iterator end;
    auto rd = FS::getPathForType(FS::PathType::RESOURCES);
    LOG(LogLevel::INFO, "Preloading resource directory: %s",
        rd.string().c_str());

    for (std::filesystem::directory_iterator it(rd); it != end; ++it) {
        if (it->is_regular_file()) {
            preloadOneFile(*it);
        }
    }
    LOG(LogLevel::INFO, "Preloading resource directory done");
}

const std::string& ResourceManager::getResource(std::filesystem::path path) {
    path.make_preferred();
    for (const auto& [elem, data] : kResources) {
        if (elem.string() == path.string()) {
            return data;
        }
    }
    LOG(LogLevel::ERROR, "Resource not found: %s", path.string().c_str());
    return empty;
}
