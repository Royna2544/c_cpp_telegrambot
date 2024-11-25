#pragma once

#include "../TgBotApiImpl.hpp"
#include "Async.hpp"
#include <map>

class TgBotApiImpl::OnCallbackQueryImpl : TgBotApiImpl::CommandListener {
    std::mutex mutex;
    std::multimap<std::string, TgBot::EventBroadcaster::CallbackQueryListener>
        listeners;
    TgBotApiImpl::Ptr _api;
    TgBotApiImpl::Async queryAsync;

    void onCallbackQueryFunction(TgBot::CallbackQuery::Ptr query);

    void onUnload(const std::string_view command) override;
    void onReload(const std::string_view command) override;
   public:
    void onCallbackQuery(
        std::string command,
        TgBot::EventBroadcaster::CallbackQueryListener listener);

    explicit OnCallbackQueryImpl(TgBotApiImpl::Ptr api);
};