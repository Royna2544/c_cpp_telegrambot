#include "ConfigFileWatcher.hpp"

#include <absl/log/log.h>

#include <condition_variable>
#include <mutex>
#include <system_error>

ConfigFileWatcher::ConfigFileWatcher(ConfigManager* configMgr)
    : configMgr_(configMgr) {
    run();
}

bool ConfigFileWatcher::mtimeChanged(
    const std::filesystem::path& path,
    std::optional<std::filesystem::file_time_type>& last) {
    std::error_code ec;
    const auto current = std::filesystem::last_write_time(path, ec);
    if (ec) {
        LOG(WARNING) << "ConfigFileWatcher: stat failed for " << path << ": "
                    << ec.message();
        return false;
    }
    const bool changed = last.has_value() && *last != current;
    last = current;
    return changed;
}

void ConfigFileWatcher::runFunction(const std::stop_token& token) {
    const auto path = configMgr_->configFilePath();
    if (!path) {
        LOG(ERROR) << "ConfigFileWatcher: no config file path resolved; "
                     "watcher exiting.";
        return;
    }

    std::optional<std::filesystem::file_time_type> lastModified;
    mtimeChanged(*path, lastModified);  // Establish the initial baseline.

    std::mutex m;
    std::condition_variable_any cv;
    while (!token.stop_requested()) {
        std::unique_lock lk(m);
        cv.wait_for(lk, token, kPollInterval, [] { return false; });
        if (token.stop_requested()) {
            break;
        }
        if (mtimeChanged(*path, lastModified)) {
            LOG(INFO) << "Config file " << *path << " changed, reloading...";
            if (configMgr_->reload()) {
                LOG(INFO) << "Config reload succeeded";
            } else {
                LOG(WARNING) << "Config reload failed, keeping previous values";
            }
        }
    }
}
