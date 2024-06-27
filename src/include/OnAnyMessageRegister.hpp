#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <cstddef>
#include <functional>
#include <map>
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
    virtual ~OnAnyMessageRegisterer() = default;

    /**
     * @brief Registers a callback function to be called when any message is
     * received.
     *
     * @param callback The function to be called when any message is received.
     */
    void registerCallback(const callback_type& callback) {
        callbacks.emplace_back(callback);
    }

    /**
     * @brief Registers a callback function with a token to be called when any
     * message is received.
     *
     * @param callback The function to be called when any message is received.
     * @param token A unique identifier for the callback.
     */
    void registerCallback(const callback_type& callback, const size_t token) {
        callbacksWithToken[token] = callback;
    }

    /**
     * @brief Unregisters a callback function with a token from the list of
     * callbacks to be called when any message is received.
     *
     * @param token The unique identifier of the callback to be unregistered.
     *
     * @return True if the callback with the specified token was found and
     * successfully unregistered, false otherwise.
     */
    bool unregisterCallback(const size_t token) {
        auto it = callbacksWithToken.find(token);
        if (it == callbacksWithToken.end()) {
            return false;
        }
        callbacksWithToken.erase(it);
        return true;
    }

    void doInitCall(Bot& bot) override {
        bot.getEvents().onAnyMessage([this, &bot](const Message::Ptr& message) {
            for (auto& callback : callbacks) {
                callback(bot, message);
            }
            for (auto& [token, callback] : callbacksWithToken) {
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
    std::map<size_t, callback_type> callbacksWithToken;
};