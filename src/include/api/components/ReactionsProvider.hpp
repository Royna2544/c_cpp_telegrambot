#pragma once

#include <api/TgBotApiImpl.hpp>

class TgBotApiImpl::ReactionsProvider {
   public:
    explicit ReactionsProvider(TgBotApi* apiImpl);

   private:
    TgBotApi* _apiImpl;

    TgBotApi::AnyMessageResult onAnyMessageFunction(Message::Ptr message);
};