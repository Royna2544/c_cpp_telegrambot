#pragma once

#include <tgbot/Bot.h>
#include <socket/TgBotSocket.h>

using TgBot::Bot;

extern void socketConnectionHandler(const Bot& bot, struct TgBotConnection);
