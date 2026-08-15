#include <api/components/ModuleManagement.hpp>
#include <future>
#include <libfs.hpp>

#include "api/CommandModule.hpp"
#include "api/TgBotApiImpl.hpp"
#include "api/components/ModuleExecutionContext.hpp"
#include "tgbot/TgException.h"

namespace {
thread_local bool inModuleControl = false;
}  // namespace

bool TgBotApiImpl::ModulesManagement::load(CommandModule::Ptr module) {
    std::string moduleName;

    // Load the library (dlopen)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (module == nullptr) {
            LOG(WARNING) << "module is null";
            return false;
        }
        if (!module->load()) {
            LOG(ERROR) << "Failed to load module";
            return false;
        }
        if (module->info.name.empty() || module->info.description.empty()) {
            LOG(ERROR) << "Empty name or description fleid";
            return false;
        }
        moduleName = module->info.name;
        if (_handles.contains(moduleName)) {
            LOG(WARNING) << fmt::format(
                "Module with name {} already loaded. REJECT", moduleName);
            return false;
        }
        _handles.emplace(moduleName, std::move(module));
    }
    // Register the command
    load(moduleName);
    return true;
}

bool TgBotApiImpl::ModulesManagement::load(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!_handles.contains(name)) {
        LOG(WARNING) << "Module with name " << name
                     << " doesn't exist to load.";
        return false;
    }
    if (!_handles.at(name)->isLoaded()) {
        if (!_handles.at(name)->load()) {
            LOG(ERROR) << "Failed to load module with name " << name;
            return false;
        }
    }

    auto accesslevel = AuthContext::AccessLevel::User;
    if (_handles.at(name)->info.isOwnerOnly()) {
        accesslevel = AuthContext::AccessLevel::Owner;
    } else if (_handles.at(name)->info.isPrivileged()) {
        accesslevel = AuthContext::AccessLevel::AdminUser;
    }

    auto* module = _handles.at(name).get();
    _api->getEvents().onCommand(name, [this, accesslevel, cmd = name,
                                       module](Message::Ptr message) {
        auto lease = module->acquireExecutionLease();
        if (!lease) {
            return;
        }
        auto leaseHolder =
            std::make_shared<RefLock::SharedLease>(std::move(*lease));
        auto prepared =
            _api->prepareCommand(cmd, accesslevel, module, std::move(message));
        if (!prepared) {
            return;
        }
        const auto rlUser = prepared->get<MessageAttrs::User>();
        const std::int64_t rlKey =
            rlUser ? rlUser->id : prepared->get<MessageAttrs::Chat>()->id;
        auto rateResult = KeyedIntervalRateLimiter::CheckResult::Allowed;
        const auto enqueueResult = commandAsync.emplaceTaskIf(
            cmd,
            [api = _api, cmd, module, prepared,
             leaseHolder = std::move(leaseHolder)] {
                module_execution::Scope active(cmd);
                api->commandHandler(cmd, module, prepared);
            },
            [this, rlKey, &rateResult] {
                rateResult = _api->_rateLimiter.checkWithStatus(rlKey);
                return rateResult ==
                           KeyedIntervalRateLimiter::CheckResult::Allowed ||
                       rateResult ==
                           KeyedIntervalRateLimiter::CheckResult::Recovered;
            });
        if (enqueueResult == Async::EnqueueResult::Rejected) {
            if (rateResult == KeyedIntervalRateLimiter::CheckResult::Limited) {
                LOG(INFO) << fmt::format("Rate limiting key {}", rlKey);
                const auto source = prepared->message();
                if (!_api->submitCommandWork(
                        cmd, TgBotApi::WorkClass::Outbound,
                        [api = _api, source](std::stop_token stop) {
                            if (!stop.stop_requested()) {
                                api->sendReplyMessage(
                                    source,
                                    "Too many commands. Please retry in a "
                                    "few seconds.");
                            }
                        },
                        {.deadline = std::chrono::seconds(10)})) {
                    LOG(WARNING)
                        << "Could not queue rate-limit feedback for " << cmd;
                }
            } else {
                DLOG(INFO) << fmt::format("Key {} remains rate limited", rlKey);
            }
        } else if (enqueueResult == Async::EnqueueResult::QueueFullOrStopping) {
            LOG(WARNING) << "Command queue is full; rejecting " << cmd;
        } else if (rateResult ==
                   KeyedIntervalRateLimiter::CheckResult::Recovered) {
            LOG(INFO) << fmt::format("Rate limit recovered for key {}", rlKey);
        }
    });
    return true;
}

bool TgBotApiImpl::ModulesManagement::unloadDirect(const std::string& name) {
    CommandModule* module = nullptr;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if (!_handles.contains(name)) {
            LOG(WARNING) << "Module with name " << name
                         << " doesn't exist to unload.";
            return false;
        }
        module = _handles.at(name).get();
        module->stopExecutions();
    }
    // Stop new command deliveries first. Remove queued command closures while
    // the DSO is still mapped, and keep this owner blocked until its active
    // command invocations and scheduler-owned work have drained.
    _api->getEvents().onCommand(name, {});
    commandAsync.cancel(name);
    workScheduler.cancelAndDrain(name);
    {
        auto executionBarrier = module->stopAndAcquireUnloadLease();
    }
    // Keep submissions blocked through the execution barrier: a command
    // delivery may have acquired its module lease just before stopExecutions()
    // and must observe rejection rather than enqueue fresh work while the
    // barrier is waiting on that lease.
    commandAsync.drain(name);

    // No code owned by this module is executing and acceptingExecutions is
    // still false. It is now safe to destroy every externally stored closure.
    for (auto* listener : _api->_listeners) {
        try {
            listener->onUnload(name);
        } catch (const std::exception& error) {
            LOG(ERROR) << "Command lifecycle listener failed while unloading "
                       << name << ": " << error.what();
        } catch (...) {
            LOG(ERROR) << "Command lifecycle listener failed while unloading "
                       << name;
        }
    }
    _api->removeAnyMessageCallbacksForCommand(name);
    if (module->isLoaded()) {
        if (!module->unload()) {
            LOG(ERROR) << "Failed to unload module with name " << name;
            return false;
        }
    }
    return true;
}

bool TgBotApiImpl::ModulesManagement::invoke(const std::string& name,
                                             Message::Ptr message) {
    CommandModule* module = nullptr;
    std::optional<RefLock::SharedLease> lease;
    AuthContext::AccessLevel accessLevel = AuthContext::AccessLevel::User;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        const auto it = _handles.find(name);
        if (it == _handles.end() || !it->second->isLoaded()) {
            LOG(WARNING) << "Cannot invoke unavailable command " << name;
            return false;
        }
        module = it->second.get();
        if (module->info.isOwnerOnly()) {
            accessLevel = AuthContext::AccessLevel::Owner;
        } else if (module->info.isPrivileged()) {
            accessLevel = AuthContext::AccessLevel::AdminUser;
        }
        // Acquire while the map is locked. Transactional reload may replace
        // and eventually destroy the object as soon as this mutex is released;
        // the lease keeps the selected generation alive until its invocation
        // has completed.
        lease = module->acquireExecutionLease();
    }

    if (!lease) {
        LOG(WARNING) << "Command stopped accepting executions: " << name;
        return false;
    }
    auto leaseHolder =
        std::make_shared<RefLock::SharedLease>(std::move(*lease));

    // The originating command already passed the fresh-update and global
    // rate-limit gates. Re-check the sender against the target command's
    // current ACL, but do not expire delayed internal work based on the
    // original Telegram timestamp.
    auto prepared = _api->prepareCommand(
        name, accessLevel, module, std::move(message),
        AuthContext::MessageAgePolicy::AuthenticatedInternal);
    if (!prepared) {
        return false;
    }

    if (!commandAsync.emplaceTask(name, [api = _api, name, module, prepared,
                                         leaseHolder = std::move(leaseHolder)] {
            module_execution::Scope active(name);
            api->commandHandler(name, module, prepared);
        })) {
        LOG(WARNING) << "Command queue is full; rejecting internal dispatch "
                     << name;
        return false;
    }
    return true;
}

bool TgBotApiImpl::ModulesManagement::runControl(
    std::string operation, std::function<bool()> function) {
    if (inModuleControl)
        return function();
    auto result = std::make_shared<std::promise<bool>>();
    auto future = result->get_future();
    if (!controlAsync.emplaceTask(
            std::move(operation), [function = std::move(function), result] {
                inModuleControl = true;
                try {
                    result->set_value(function());
                } catch (...) {
                    try {
                        result->set_exception(std::current_exception());
                    } catch (...) {
                    }
                }
                inModuleControl = false;
            })) {
        LOG(WARNING) << "Module control queue is full";
        return false;
    }
    try {
        return future.get();
    } catch (const std::exception& error) {
        LOG(ERROR) << "Module control operation failed: " << error.what();
        return false;
    }
}

bool TgBotApiImpl::ModulesManagement::unload(const std::string& name) {
    if (module_execution::isExecuting(name)) {
        LOG(WARNING) << "Rejecting self-unload for command " << name;
        return false;
    }
    const auto caller = module_execution::currentOwner();
    if (!caller.empty() && caller != "cmd") {
        LOG(WARNING) << "Rejecting module lifecycle request from executing "
                     << caller;
        return false;
    }
    return runControl("unload:" + name,
                      [this, name] { return unloadDirect(name); });
}

bool TgBotApiImpl::ModulesManagement::reloadDirect(const std::string& name) {
    std::unique_ptr<DynCommandModule> replacement;
    CommandModule* current = nullptr;
    {
        const std::lock_guard lock(mutex);
        const auto found = _handles.find(name);
        if (found == _handles.end() || !found->second->isLoaded())
            return false;
        auto* dynamic = dynamic_cast<DynCommandModule*>(found->second.get());
        if (!dynamic) {
            LOG(WARNING) << "Transactional reload only supports dynamic "
                            "command modules: "
                         << name;
            return false;
        }
        current = found->second.get();
        replacement = dynamic->makeReloadCandidate();
    }
    if (!replacement || !replacement->isLoaded() ||
        replacement->info.name != name) {
        LOG(ERROR) << "Reload preflight failed for " << name;
        return false;
    }

    // Keep the old image mapped until the replacement is registered. This is
    // the rollback boundary: unloading first would make "rollback" reopen the
    // file currently on disk, which may already be the broken new image.
    replacement->stopExecutions();
    current->stopExecutions();
    _api->getEvents().onCommand(name, {});
    // Block the owner before changing generations. Queued closures refer to
    // the previous module, so destroy them while that DSO is still mapped.
    commandAsync.cancel(name);

    CommandModule::Ptr previous;
    bool missingCurrent = false;
    {
        const std::lock_guard lock(mutex);
        auto found = _handles.find(name);
        if (found == _handles.end()) {
            missingCurrent = true;
        } else {
            previous = std::move(found->second);
            found->second = std::move(replacement);
        }
    }
    if (missingCurrent) {
        commandAsync.drain(name);
        return false;
    }
    if (!load(name)) {
        LOG(ERROR) << "Reload registration failed for " << name
                   << "; restoring the still-mapped previous module";
        CommandModule::Ptr failedCandidate;
        CommandModule* restored = nullptr;
        bool missingCandidate = false;
        {
            const std::lock_guard lock(mutex);
            auto found = _handles.find(name);
            if (found == _handles.end()) {
                missingCandidate = true;
            } else {
                failedCandidate = std::move(found->second);
                found->second = std::move(previous);
                restored = found->second.get();
            }
        }
        if (missingCandidate) {
            commandAsync.drain(name);
            return false;
        }
        // Balance cancel() only after every delivery that acquired a lease
        // before the reload fence has returned. The restored generation stays
        // stopped until then, so no late closure can slip into the queue.
        {
            auto executionBarrier = restored->stopAndAcquireUnloadLease();
        }
        commandAsync.drain(name);
        {
            const std::lock_guard lock(mutex);
            const auto found = _handles.find(name);
            if (found == _handles.end())
                return false;
            found->second->enableExecutions();
        }
        if (failedCandidate && failedCandidate->isLoaded())
            (void)failedCandidate->unload();
        return load(name);
    }

    // The candidate command listener is installed but deliberately stopped,
    // so no new-generation work can be confused with cleanup of the previous
    // generation. Cancel old scheduler work, drain active command closures,
    // then wait for every remaining callback execution lease before removing
    // stored DSO closures.
    workScheduler.cancelAndDrain(name);
    {
        auto executionBarrier = previous->stopAndAcquireUnloadLease();
    }
    commandAsync.drain(name);
    for (auto* listener : _api->_listeners) {
        try {
            listener->onUnload(name);
        } catch (const std::exception& error) {
            LOG(ERROR) << "Command lifecycle listener failed while reloading "
                       << name << ": " << error.what();
        } catch (...) {
            LOG(ERROR) << "Command lifecycle listener failed while reloading "
                       << name;
        }
    }
    _api->removeAnyMessageCallbacksForCommand(name);
    if (previous->isLoaded() && !previous->unload()) {
        // All old callbacks and work have been drained, so retaining the old
        // handle briefly is safe; its RAII destructor will make one final
        // close attempt without taking the replacement offline.
        LOG(ERROR) << "Could not eagerly close previous module image for "
                   << name;
    }

    {
        const std::lock_guard lock(mutex);
        const auto found = _handles.find(name);
        if (found == _handles.end())
            return false;
        found->second->enableExecutions();
    }
    for (auto* listener : _api->_listeners) {
        try {
            listener->onReload(name);
        } catch (const std::exception& error) {
            LOG(ERROR) << "Command lifecycle listener failed after reloading "
                       << name << ": " << error.what();
        } catch (...) {
            LOG(ERROR) << "Command lifecycle listener failed after reloading "
                       << name;
        }
    }
    return true;
}

bool TgBotApiImpl::ModulesManagement::reload(const std::string& name) {
    if (module_execution::isExecuting(name)) {
        LOG(WARNING) << "Rejecting self-reload for command " << name;
        return false;
    }
    const auto caller = module_execution::currentOwner();
    if (!caller.empty() && caller != "cmd") {
        LOG(WARNING) << "Rejecting module lifecycle request from executing "
                     << caller;
        return false;
    }
    return runControl("reload:" + name,
                      [this, name] { return reloadDirect(name); });
}

std::shared_ptr<RefLock::SharedLease>
TgBotApiImpl::ModulesManagement::acquireExecutionLease(
    const std::string_view owner) {
    const std::lock_guard lock(mutex);
    const auto found = _handles.find(std::string(owner));
    if (found == _handles.end()) {
        return {};
    }
    auto lease = found->second->acquireExecutionLease();
    if (!lease) {
        return {};
    }
    return std::make_shared<RefLock::SharedLease>(std::move(*lease));
}

std::optional<TgBotApi::WorkId> TgBotApiImpl::ModulesManagement::submitWork(
    std::string_view owner, TgBotApi::WorkClass workClass,
    TgBotApi::CancellableWork work, TgBotApi::WorkOptions options) {
    std::shared_ptr<RefLock::SharedLease> leaseHolder;
    {
        const std::lock_guard lock(mutex);
        const auto found = _handles.find(std::string(owner));
        if (found == _handles.end() || !found->second->isLoaded()) {
            LOG(WARNING) << "Cannot submit work for unavailable module "
                         << owner;
            return std::nullopt;
        }
        auto lease = found->second->acquireExecutionLease();
        if (!lease) {
            LOG(WARNING) << "Module is stopping; rejecting work for " << owner;
            return std::nullopt;
        }
        leaseHolder = std::make_shared<RefLock::SharedLease>(std::move(*lease));
    }
    return workScheduler.submit(std::string(owner), workClass, std::move(work),
                                options, std::move(leaseHolder));
}

bool TgBotApiImpl::ModulesManagement::cancelWork(std::string_view owner,
                                                 TgBotApi::WorkId id) {
    return workScheduler.cancel(owner, id);
}

bool TgBotApiImpl::ModulesManagement::loadAll(
    const std::filesystem::path& directory) {
    std::error_code ec;

    LOG(INFO) << "Loading commands from " << directory;
    for (const auto& it : std::filesystem::directory_iterator(directory, ec)) {
        const auto filename = it.path().filename();
        if (filename.string().starts_with(DynCommandModule::prefix) &&
            filename.extension() == FS::kDylibExtension) {
            if (load(std::make_unique<DynCommandModule>(it)))
                continue;
        }
#ifdef HAVE_LUA
        if (filename.extension() == ".lua") {
            if (load(std::make_unique<LuaCommandModule>(it)))
                continue;
        }
#endif
        DLOG(WARNING) << "Not loading " << filename;
    }
    if (ec) {
        LOG(ERROR) << "Failed to iterate through modules: " << ec.message();
        return false;
    }

    extern const struct DynModule cmd_kernelbuild;
    extern const struct DynModule cmd_rombuild;
    load(std::make_unique<BuiltinCommandModule>(&cmd_kernelbuild));
    load(std::make_unique<BuiltinCommandModule>(&cmd_rombuild));

    // Update BotCommandList
    std::vector<TgBot::BotCommand::Ptr> user_commands;
    std::vector<TgBot::BotCommand::Ptr> privileged_commands;
    user_commands.reserve(_handles.size());
    for (const auto& [name, mod] : _handles) {
        if (!mod->info.isHideDescription()) {
            auto onecommand = std::make_shared<TgBot::BotCommand>();
            onecommand->command = mod->info.name;
            onecommand->description = mod->info.description;
            if (mod->info.isPrivileged()) {
                privileged_commands.emplace_back(onecommand);
            } else {
                user_commands.emplace_back(onecommand);
            }
        }
    }
    try {
        if (!user_commands.empty())
            _api->getApi().setMyCommands(user_commands);
        if (auto id = _api->_provider->database->getOwnerUserId();
            id && !privileged_commands.empty()) {
            auto owner_scope = std::make_shared<TgBot::BotCommandScopeChat>();
            owner_scope->chatId = id.value();
            _api->getApi().setMyCommands(privileged_commands, owner_scope);
        }
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << fmt::format("Error updating bot commands list: {}",
                                  e.what());
        return false;
    } catch (const TgBot::NetworkException& e) {
        LOG(ERROR) << fmt::format(
            "Network error on updating bot commands list: {}", e.what());
        return false;
    }
    return true;
}

TgBotApiImpl::ModulesManagement::ModulesManagement(
    TgBotApiImpl::Ptr api, const std::filesystem::path& modules_dir)
    : _api(api),
      controlAsync("module-control", 1, 8),
      commandAsync("commands", 2) {
    loadAll(modules_dir);
}

TgBotApiImpl::ModulesManagement::~ModulesManagement() {
    LOG(INFO) << "Unloading total " << _handles.size() << " modules";
    // First stop every module and unregister every command. This prevents one
    // still-running module from dispatching fresh work into another while the
    // second pass drains execution leases and destroys callbacks.
    for (const auto& [name, module] : _handles) {
        module->stopExecutions();
        _api->getEvents().onCommand(name, {});
        // Cancel every owner's queued fast-command work before waiting on any
        // owner. Otherwise the two command workers can both be occupied by
        // control calls whose control task is waiting for a queued lease.
        commandAsync.cancel(name);
    }

    // Restart explicitly destroys the module manager before the callback
    // registries. Drain all code from each DSO before destroying any closure
    // whose std::function manager may reside in that DSO.
    for (const auto& [name, module] : _handles) {
        workScheduler.cancelAndDrain(name);
        {
            auto executionBarrier = module->stopAndAcquireUnloadLease();
        }
        commandAsync.drain(name);
        for (auto* listener : _api->_listeners) {
            try {
                listener->onUnload(name);
            } catch (const std::exception& error) {
                LOG(ERROR) << "Command lifecycle listener failed during "
                              "shutdown for "
                           << name << ": " << error.what();
            } catch (...) {
                LOG(ERROR) << "Command lifecycle listener failed during "
                              "shutdown for "
                           << name;
            }
        }
        _api->removeAnyMessageCallbacksForCommand(name);
    }
}
