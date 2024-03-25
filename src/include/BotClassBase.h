#pragma once

#include <tgbot/Bot.h>

struct BotClassBase {
    BotClassBase(const TgBot::Bot& bot) : _bot(bot) {}
    BotClassBase() = delete;
   protected:
    const TgBot::Bot& _bot;
};