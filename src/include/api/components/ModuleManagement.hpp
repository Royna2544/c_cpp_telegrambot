#include <mutex>
#include <unordered_map>

#include "../TgBotApiImpl.hpp"
#include "Async.hpp"
#include "WorkScheduler.hpp"
#include "api/CommandModule.hpp"

class TgBotApiImpl::ModulesManagement {
    std::unordered_map<std::string, CommandModule::Ptr> _handles;
    TgBotApiImpl::Ptr _api;
    mutable std::mutex mutex;

    TgBotApiImpl::Async controlAsync;
    TgBotApiImpl::Async commandAsync;
    TgBotApiImpl::WorkScheduler workScheduler;

    bool unloadDirect(const std::string& name);
    bool reloadDirect(const std::string& name);
    bool runControl(std::string operation, std::function<bool()> function);

   public:
    // Load module by `name' and add it to the management.
    bool load(CommandModule::Ptr module);
    // (Re) load module by `name' from the management modules.
    bool load(const std::string& name);
    // Unload module by `name' from the management modules.
    bool unload(const std::string& name);
    bool reload(const std::string& name);
    // Dispatch a loaded module using an existing authenticated message.
    bool invoke(const std::string& name, Message::Ptr message);
    std::shared_ptr<RefLock::SharedLease> acquireExecutionLease(
        std::string_view owner);
    std::optional<TgBotApi::WorkId> submitWork(std::string_view owner,
                                               TgBotApi::WorkClass workClass,
                                               TgBotApi::CancellableWork work,
                                               TgBotApi::WorkOptions options);
    bool cancelWork(std::string_view owner, TgBotApi::WorkId id);
    // Load all modules from the directory.
    bool loadAll(const std::filesystem::path& directory);

    explicit ModulesManagement(TgBotApiImpl::Ptr api,
                               const std::filesystem::path& modules_dir);
    ~ModulesManagement();
};
