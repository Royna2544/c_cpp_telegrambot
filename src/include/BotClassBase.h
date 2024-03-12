#include <tgbot/Bot.h>

struct BotClassBase {
    BotClassBase(const TgBot::Bot& bot) : _bot(bot) {}
   protected:
    const TgBot::Bot& _bot;
};