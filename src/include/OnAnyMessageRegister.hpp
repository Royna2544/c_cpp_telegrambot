#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <functional>
#include <string_view>
#include <vector>

#include "Authorization.h"
#include "CStringLifetime.h"
#include "CompileTimeStringConcat.hpp"
#include "InstanceClassBase.hpp"
#include "initcalls/BotInitcall.hpp"

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
    const CStringLifetime getInitCallName() const override {
        return "Register onAnyMessage callbacks";
    }

    template <unsigned Len>
    static consteval auto getInitCallNameForClient(const char (&str)[Len]) {
        return StringConcat::cat("Register onAnyMessage callbacks: ", str);
    }

   private:
    std::vector<callback_type> callbacks;
};