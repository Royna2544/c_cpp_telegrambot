#pragma once

#include <ManagedThreads.hpp>

#include <chrono>
#include <filesystem>
#include <optional>
#include <stop_token>

#include "ConfigManager.hpp"

// Polls the config file's mtime and calls ConfigManager::reload() whenever
// it changes on disk, so a hand-edit takes effect without a bot restart.
// Only config values re-read fresh on every use (e.g. LLM_URL/LLM_API_TYPE/
// LLM_AUTHKEY) actually benefit - see ConfigManager::reload()'s doc comment
// for the values that don't.
class ConfigFileWatcher : public ThreadRunner {
   public:
    explicit ConfigFileWatcher(ConfigManager* configMgr);
    ~ConfigFileWatcher() override = default;

   private:
    void runFunction(const std::stop_token& token) override;

    // Returns true (and updates the cached timestamp) if `path`'s mtime
    // changed since the last call. Same polling idiom as FileWithTimestamp
    // (src/api/builtin_modules/builder/FileWithTimestamp.hpp), reimplemented
    // locally to avoid a utils/ -> api/builtin_modules/builder dependency.
    static bool mtimeChanged(const std::filesystem::path& path,
                            std::optional<std::filesystem::file_time_type>& last);

    ConfigManager* configMgr_;
    static constexpr std::chrono::seconds kPollInterval{3};
};
