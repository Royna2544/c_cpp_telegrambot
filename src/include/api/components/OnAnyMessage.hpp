#pragma once

#include "../TgBotApiImpl.hpp"

#include "Async.hpp"

class TgBotApiImpl::OnAnyMessageImpl {
    std::mutex mutex;
    std::vector<TgBotApi::AnyMessageCallback> callbacks;
    TgBotApiImpl::Ptr _api;

    void onAnyMessageFunction(Message::Ptr message);
   public:
    /**
     * @brief Registers a callback function to be called when any message is
     * received.
     *
     * @param callback The function to be called when any message is
     * received.
     */
    void onAnyMessage(const TgBotApi::AnyMessageCallback& callback);

    explicit OnAnyMessageImpl(TgBotApiImpl::Ptr api);
};