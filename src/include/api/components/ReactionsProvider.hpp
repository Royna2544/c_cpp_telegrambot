#pragma once

#include <api/TgBotApiImpl.hpp>

class TgBotApiImpl::ReactionsProvider {
   public:
    explicit ReactionsProvider(TgBotApi* apiImpl);
    ~ReactionsProvider();

   private:
    TgBotApi* _apiImpl;
    TgBotApi::CallbackSubscription::Ptr anyMessageSubscription_;

    TgBotApi::AnyMessageResult onAnyMessageFunction(Message::Ptr message);
};
