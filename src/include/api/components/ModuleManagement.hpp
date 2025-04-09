#include <unordered_map>
#include <mutex>

#include "../TgBotApiImpl.hpp"
#include "Async.hpp"
#include "api/CommandModule.hpp"

class TgBotApiImpl::ModulesManagement {
    std::unordered_map<std::string, CommandModule::Ptr> _handles;
    TgBotApiImpl::Ptr _api;
    mutable std::mutex mutex;

    TgBotApiImpl::Async commandAsync;

   public:
    // Check if module by `name' is present.
    CommandModule* operator[](const std::string& name) const;
    // Load module by `name' and add it to the management.
    bool load(CommandModule::Ptr module);
    // (Re) load module by `name' from the management modules.
    bool load(const std::string& name);
    // Unload module by `name' from the management modules.
    bool unload(const std::string& name);
    // Load all modules from the directory.
    bool loadAll(const std::filesystem::path& directory);

    explicit ModulesManagement(TgBotApiImpl::Ptr api,
                               const std::filesystem::path& modules_dir);
    ~ModulesManagement();
};
