#include <absl/log/log.h>
#include <tgbot/TgException.h>

#include <api/components/OnAnyMessage.hpp>
#include <future>
#include "Authorization.hpp"

void TgBotApiImpl::OnAnyMessageImpl::onAnyMessage(
    const TgBotApi::AnyMessageCallback& callback) {
    const std::lock_guard<std::mutex> _(mutex);
    callbacks.emplace_back(callback);
}

void TgBotApiImpl::OnAnyMessageImpl::onAnyMessageFunction(
    Message::Ptr message) {
    decltype(callbacks)::const_reverse_iterator it;
    std::vector<
        std::pair<std::future<TgBotApi::AnyMessageResult>, decltype(it)>>
        vec;
    const std::lock_guard<std::mutex> lock(mutex);

    if (callbacks.empty()) {
        return;
    }
    it = callbacks.crbegin();
    while (it != callbacks.crend()) {
        const auto fn_copy = *it;
        vec.emplace_back(std::async(std::launch::async, fn_copy, _api, message),
                         it++);
    }

    for (auto& [future, callback] : vec) {
        try {
            switch (future.get()) {
                // Skip
                case TgBotApi::AnyMessageResult::Handled:
                    break;

                case TgBotApi::AnyMessageResult::Deregister:
                    callbacks.erase(callback.base());
                    break;
            }
        } catch (const TgBot::TgException& ex) {
            LOG(ERROR) << "Error in onAnyMessageCallback: " << ex.what();
        }
    }
}

TgBotApiImpl::OnAnyMessageImpl::OnAnyMessageImpl(TgBotApiImpl::Ptr api)
    : _api(api) {
    _api->getEvents().onAnyMessage([this](Message::Ptr message) {
        if (!AuthContext::isUnderTimeLimit(message)) {
            return;
        }
        onAnyMessageFunction(std::move(message));
    });
}