#pragma once

#include <api/TgBotApiImpl.hpp>
#include <api/types/CallbackQuery.hpp>
#include <api/types/ChatJoinRequest.hpp>
#include <api/types/ChatMembers.hpp>
#include <api/types/Message.hpp>

class TgBotApiImpl::ChatJoinRequestImpl {
    std::vector<std::pair<api::types::Message, api::types::ChatJoinRequest>>
        joinReqs;
    std::optional<api::types::InlineKeyboardMarkup> templateMarkup;
    TgBotApiImpl::Ptr _api;
    std::mutex mutex;

    void onChatJoinRequestFunction(api::types::ChatJoinRequest ptr);
    void onCallbackQueryFunction(const api::types::CallbackQuery& query);
    void onChatMemberFunction(const api::types::ChatMemberUpdated& update);

   public:
    explicit ChatJoinRequestImpl(TgBotApiImpl::Ptr api);
};