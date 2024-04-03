#pragma once

#include <tgbot/Bot.h>

#include <absl/log/log.h>
#include <chrono>
#include "internal/_std_chrono_templates.h"

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
        DLOG(INFO) << getInitCallName() << ": +++";
        auto now = std::chrono::high_resolution_clock::now();
        doInitCall(bot);
        auto elapsed = to_msecs(std::chrono::high_resolution_clock::now() - now);
        DLOG(INFO) << getInitCallName() << ": --- (" << elapsed.count() << "ms)";
    }
};