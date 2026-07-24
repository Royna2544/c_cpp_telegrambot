#include <api/components/OnCallbackQuery.hpp>
#include <api/components/ModuleManagement.hpp>

void TgBotApiImpl::OnCallbackQueryImpl::onCallbackQueryFunction(
    TgBot::CallbackQuery::Ptr query) {
    const std::lock_guard<std::mutex> lock(mutex);
    if (listeners.empty()) {
        return;
    }
    for (const auto& [command, callback] : listeners) {
        auto* module = (*_api->kModuleLoader)[command];
        if (module == nullptr) {
            continue;
        }
        auto lease = module->acquireExecutionLease();
        if (!lease) {
            continue;
        }
        auto leaseHolder = std::make_shared<RefLock::SharedLease>(
            std::move(*lease));
        if (!queryAsync.emplaceTask(
                command, [callback, query,
                          leaseHolder = std::move(leaseHolder)] {
                    callback(query);
                })) {
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
