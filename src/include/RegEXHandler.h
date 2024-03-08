#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

using TgBot::Bot;
using TgBot::Message;

/**
 * @brief Processes a regular expression command
 * 
 * @param bot the bot instance
 * @param msg the incoming message
 */
void processRegEXCommand(const Bot& bot, const Message::Ptr& msg);
