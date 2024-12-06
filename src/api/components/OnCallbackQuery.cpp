#include <api/components/OnCallbackQuery.hpp>

void TgBotApiImpl::OnCallbackQueryImpl::onCallbackQueryFunction(
    TgBot::CallbackQuery::Ptr query) {
    const std::lock_guard<std::mutex> lock(mutex);
    if (listeners.empty()) {
        return;
    }
    for (const auto& [command, callback] : listeners) {
        queryAsync.emplaceTask(command,
                               std::async(std::launch::async, callback, query));
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
    for (auto it = listeners.begin(); it != listeners.end(); ++it) {
        if (it->first == command) {
            DLOG(INFO) << "Removing callback query handler for " << command;
            listeners.erase(it);
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