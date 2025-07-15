#include <absl/log/log.h>
#include <tgbot/TgException.h>

#include <api/AuthContext.hpp>
#include <api/components/OnAnyMessage.hpp>
#include <future>

void TgBotApiImpl::OnAnyMessageImpl::onAnyMessage(
    const TgBotApi::AnyMessageCallback& callback) {
    const std::lock_guard<std::mutex> _(mutex);
    callbacks.emplace_back(callback);
}

void TgBotApiImpl::OnAnyMessageImpl::onAnyMessageFunction(
    Message::Ptr message) {
    std::map<int, std::future<TgBotApi::AnyMessageResult>> vec;
    const std::lock_guard<std::mutex> lock(mutex);

    if (callbacks.empty()) {
        return;
    }
    int index = 0;
    for (const auto& iter : callbacks) {
        vec.emplace(index++,
                    std::async(std::launch::async, iter, _api, message));
    }

    index = 0;
    auto [b, e] = std::ranges::remove_if(callbacks, [&](const auto& result) {
        try {
            switch (vec[index++].get()) {
                // Skip
                case TgBotApi::AnyMessageResult::Handled:
                    return false;

                case TgBotApi::AnyMessageResult::Deregister:
                    return true;
            }
        } catch (const TgBot::TgException& ex) {
            LOG(ERROR) << "Error in onAnyMessageCallback: " << ex.what();
            LOG(INFO) << "Deregistering handler (As further errors can occur)";
        }
        return true;
    });
    callbacks.erase(b, e);
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
