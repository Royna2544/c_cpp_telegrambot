#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "../TgBotApiImpl.hpp"

namespace tgbot::detail {

// Runs any-message callbacks on a small, bounded worker set. Each callback has
// its own FIFO strand, so different callbacks may make progress concurrently
// while a single callback is never re-entered for two messages at once.
class AnyMessageCallbackDispatcher final {
   public:
    explicit AnyMessageCallbackDispatcher(
        TgBotApiImpl::Ptr api, std::size_t workerCount = 2,
        std::size_t maxPendingInvocations = 256);
    ~AnyMessageCallbackDispatcher();

    AnyMessageCallbackDispatcher(const AnyMessageCallbackDispatcher&) = delete;
    AnyMessageCallbackDispatcher& operator=(
        const AnyMessageCallbackDispatcher&) = delete;

    [[nodiscard]] bool registerCallback(
        std::string ownerCommand, const TgBotApi::AnyMessageCallback& callback);
    [[nodiscard]] TgBotApi::CallbackSubscription::Ptr subscribeCallback(
        const TgBotApi::AnyMessageCallback& callback);
    [[nodiscard]] bool enqueue(Message::Ptr message);
    void removeCallbacksForCommand(std::string_view command);

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace tgbot::detail

class TgBotApiImpl::OnAnyMessageImpl {
    TgBotApiImpl::Ptr _api;
    tgbot::detail::AnyMessageCallbackDispatcher dispatcher_;

    void onAnyMessageFunction(Message::Ptr message);

   public:
    /**
     * @brief Registers a callback function to be called when any message is
     * received.
     *
     * @param callback The function to be called when any message is
     * received.
     */
    void onAnyMessage(const TgBotApi::AnyMessageCallback& callback,
                      std::string ownerCommand = {});
    [[nodiscard]] TgBotApi::CallbackSubscription::Ptr subscribeAnyMessage(
        const TgBotApi::AnyMessageCallback& callback);
    [[nodiscard]] TgBotApi::CallbackSubscription::Ptr subscribeEditedMessage(
        TgBot::EventBroadcaster::MessageListener listener);
    void removeCallbacksForCommand(std::string_view command);

    explicit OnAnyMessageImpl(TgBotApiImpl::Ptr api);
};
