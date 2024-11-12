#include <api/TgBotApiImpl.hpp>

#include "tgbot/types/CallbackQuery.h"

class TgBotApiImpl::ChatJoinRequestImpl {
    std::vector<std::pair<Message::Ptr, TgBot::ChatJoinRequest::Ptr>> joinReqs;
    TgBot::InlineKeyboardMarkup::Ptr templateMarkup;
    TgBotApiImpl::Ptr _api;
    std::mutex mutex;

    void onChatJoinRequestFunction(TgBot::ChatJoinRequest::Ptr ptr);
    void onCallbackQueryFunction(const TgBot::CallbackQuery::Ptr& query);
    void onChatMemberFunction(const TgBot::ChatMemberUpdated::Ptr& update);

   public:
    explicit ChatJoinRequestImpl(TgBotApiImpl::Ptr api);
};