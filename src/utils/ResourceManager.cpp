#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <fmt/format.h>

#include <ResourceManager.hpp>
#include <filesystem>
#include <fstream>
#include <libfs.hpp>
#include <sstream>
#include <system_error>
#include <utility>

bool ResourceManager::preload(std::filesystem::path path) {
    std::ifstream file(path);
    std::stringstream linebuf;
    bool found = false;

    path.make_preferred();
    for (auto p = path.parent_path(); p != path.root_path();
         p = p.parent_path()) {
        if (std::filesystem::equivalent(p, m_resourceDirectory)) {
            found = true;
            break;
        }
    }
    if (found) {
        path = path.lexically_relative(m_resourceDirectory);
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

ResourceManager::ResourceManager(std::filesystem::path kResourceDirectory)
    : m_resourceDirectory(std::move(kResourceDirectory)) {
    std::ifstream ifs(m_resourceDirectory / kResourceLoadIgnoreFile);
    std::error_code ec;

    if (ifs) {
        std::string line;
        while (std::getline(ifs, line)) {
            absl::StripAsciiWhitespace(&line);
            if (!line.empty()) {
                ignoredResources.emplace_back(line);
            }
        }
        ignoredResources.emplace_back(kResourceLoadIgnoreFile);
        LOG(INFO) << "Applied resource ignore configuration";
    }

    LOG(INFO) << "Preloading resource directory: " << m_resourceDirectory;
    for (const auto& it :
         std::filesystem::directory_iterator(m_resourceDirectory, ec)) {
        if (it.is_regular_file()) {
            preload(it);
        }
    }
    if (ec) {
        LOG(ERROR) << "Failed to preload resource directory: " << ec.message();
    } else {
        LOG(INFO) << "Preloading resource directory done";
    }
}

std::string_view ResourceManager::get(std::filesystem::path filename) const {
    filename.make_preferred();
    for (const auto& [elem, data] : kResources) {
        if (elem.string() == filename.string()) {
            return data;
        }
    }
    LOG(ERROR) << "Resource not found: " << filename;
    throw std::runtime_error(
        fmt::format("Resource not found: {}", filename.string()));
}
