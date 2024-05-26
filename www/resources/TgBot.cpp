#include <tgbot/tgbot.h>
#include <cstdint>

int main() {
    TgBot::Bot bot("token!");
    int64_t yourId = 123123123;

    // Sends hello to your PM!
    bot.getApi().sendMessage(yourId, "Hello");
}
