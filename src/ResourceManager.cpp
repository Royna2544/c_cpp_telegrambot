#include <Logging.h>
#include <ResourceManager.h>
#include <popen_wdt/popen_wdt.hpp>
#include <fstream>
#include <sstream>

bool ResourceManager::preloadOneFile(std::filesystem::path path) {
    std::ifstream file(path);
    std::stringstream linebuf;

    path.make_preferred();
    if (path.is_absolute())
        path = path.lexically_relative(getResourceRootdir());

    for (const auto& [elem, data] : kResources) {
        if (elem.string() == path.string()) {
            LOG_W("Resource %s already loaded", path.string().c_str());
            return false;
        }
    }
    LOG_V("Preloading %s", path.string().c_str());
    linebuf << file.rdbuf();
    kResources[path] = linebuf.str();
    return true;
}

void ResourceManager::preloadResourceDirectory() {
    LOG_V("Preloading resource directory");
    std::filesystem::recursive_directory_iterator end;
    for (std::filesystem::recursive_directory_iterator it(getResourceRootdir()); it != end; ++it) {
        if (it->is_regular_file()) {
            preloadOneFile(*it);
        }
    }
    LOG_V("Preloading resource directory done");
}

const std::string& ResourceManager::getResource(std::filesystem::path path) {
    path.make_preferred();
    for (const auto& [elem, data] : kResources) {
        if (elem.string() == path.string()) {
            return data;
        }
    }
    LOG_E("Resource not found: %s", path.string().c_str());
    return empty;
}

std::filesystem::path ResourceManager::getResourceRootdir() {
    static auto path = getSrcRoot() / "resources";
    return path;
}

const std::string ResourceManager::empty = "";

ResourceManager gResourceManager;