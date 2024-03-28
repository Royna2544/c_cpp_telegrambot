#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <functional>
#include <vector>

#include "Authorization.h"
#include "initcalls/BotInitcall.hpp"
#include "InstanceClassBase.hpp"

using TgBot::Bot;
using TgBot::Message;

struct OnAnyMessageRegisterer : InstanceClassBase<OnAnyMessageRegisterer>,
                                BotInitCall {
    using callback_type = std::function<void(const Bot&, const Message::Ptr&)>;
    explicit OnAnyMessageRegisterer() = default;
    OnAnyMessageRegisterer& registerCallback(const callback_type& callback) {
        callbacks.emplace_back(callback);
        return *this;
    }
    void doInitCall(Bot& bot) override {
        bot.getEvents().onAnyMessage([this, &bot](const Message::Ptr& message) {
            for (auto& callback : callbacks) {
                callback(bot, message);
            }
        });
    }
    const char* getInitCallName() const override {
        return "Register onAnyMessage callbacks";
    }

   private:
    std::vector<callback_type> callbacks;
};