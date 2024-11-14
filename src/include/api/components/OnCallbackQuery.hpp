#pragma once

#include "../TgBotApiImpl.hpp"
#include "Async.hpp"
#include <map>

class TgBotApiImpl::OnCallbackQueryImpl {
    std::mutex mutex;
    std::multimap<std::string, TgBot::EventBroadcaster::CallbackQueryListener>
        listeners;
    TgBotApiImpl::Ptr _api;
    TgBotApiImpl::Async queryAsync;

    void onCallbackQueryFunction(TgBot::CallbackQuery::Ptr query);

   public:
    void onCallbackQuery(
        std::string command,
        TgBot::EventBroadcaster::CallbackQueryListener listener);

    explicit OnCallbackQueryImpl(TgBotApiImpl::Ptr api);
};