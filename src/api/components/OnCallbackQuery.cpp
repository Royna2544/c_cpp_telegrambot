#include <api/components/ModuleExecutionContext.hpp>
#include <api/components/ModuleManagement.hpp>
#include <api/components/OnCallbackQuery.hpp>

namespace {
struct CallbackInvocation {
    std::string owner;
    std::shared_ptr<RefLock::SharedLease> moduleLease;
    TgBot::EventBroadcaster::CallbackQueryListener callback;
    TgBot::CallbackQuery::Ptr query;

    ~CallbackInvocation() {
        // The std::function manager may be implemented in the command DSO.
        // Destroy it before releasing the lease which permits dlclose.
        callback = {};
        query.reset();
        moduleLease.reset();
    }

    void operator()() const {
        module_execution::Scope scope(owner);
        callback(query);
    }
};
}  // namespace

void TgBotApiImpl::OnCallbackQueryImpl::onCallbackQueryFunction(
    TgBot::CallbackQuery::Ptr query) {
    const std::lock_guard<std::mutex> lock(mutex);
    if (listeners.empty()) {
        return;
    }
    for (const auto& [command, callback] : listeners) {
        if (!_api->kModuleLoader) {
            continue;
        }
        auto leaseHolder = _api->kModuleLoader->acquireExecutionLease(command);
        if (!leaseHolder) {
            continue;
        }
        auto invocation = std::make_shared<CallbackInvocation>();
        invocation->owner = command;
        invocation->moduleLease = std::move(leaseHolder);
        invocation->callback = callback;
        invocation->query = query;
        if (!queryAsync.emplaceTask(command,
                                    [invocation] { (*invocation)(); })) {
            LOG(WARNING) << "Callback-query queue is full; rejecting "
                         << command;
        }
    }
}

void TgBotApiImpl::OnCallbackQueryImpl::onCallbackQuery(
    std::string command,
    TgBot::EventBroadcaster::CallbackQueryListener listener) {
    const std::lock_guard<std::mutex> _(mutex);
    listeners.emplace(std::move(command), std::move(listener));
}

void TgBotApiImpl::OnCallbackQueryImpl::onUnload(
    const std::string_view command) {
    const std::lock_guard<std::mutex> lock(mutex);
    for (auto it = listeners.begin(); it != listeners.end();) {
        if (it->first == command) {
            DLOG(INFO) << "Removing callback query handler for " << command;
            it = listeners.erase(it);
        } else {
            ++it;
        }
    }
}

void TgBotApiImpl::OnCallbackQueryImpl::onReload(
    const std::string_view /*command*/) {}

TgBotApiImpl::OnCallbackQueryImpl::OnCallbackQueryImpl(TgBotApiImpl::Ptr api)
    : _api(api), queryAsync("callbackquery", 2) {
    _api->getEvents().onCallbackQuery([this](TgBot::CallbackQuery::Ptr query) {
        onCallbackQueryFunction(std::move(query));
    });
    _api->addCommandListener(this);
}
