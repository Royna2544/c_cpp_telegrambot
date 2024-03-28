#pragma once

#include <tgbot/Bot.h>

#include "Logging.h"

using TgBot::Bot;

struct BotInitCall {
    /**
     * @brief A virtual function that is called when the bot is initialized
     *
     * @param bot a reference to the bot instance
     */
    virtual void doInitCall(Bot& bot) = 0;

    /**
     * @brief A virtual function that will return what is initcall doing
     *
     * @return The name of the initcall's work
     */
    virtual const char* getInitCallName() const = 0;

    void initWrapper(Bot& bot) {
        LOG(LogLevel::VERBOSE, "%s: +++", getInitCallName());
        doInitCall(bot);
        LOG(LogLevel::VERBOSE, "%s: ---", getInitCallName());
    }
};