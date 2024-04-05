#pragma once

#include <tgbot/Bot.h>

#include "InitcallBase.hpp"

using TgBot::Bot;

struct BotInitCall : InitcallBase {
    /**
     * @brief A virtual function that is called when the bot is initialized
     *
     * @param bot a reference to the bot instance
     */
    virtual void doInitCall(Bot& bot) = 0;

    void initWrapper(Bot& bot) {
        auto dp = onStart();
        doInitCall(bot);
        onEnd(dp);
    }
};