#pragma once

#include <tgbot/Bot.h>
#include <filesystem>

using TgBot::Bot;

/**
 * @brief loads a single command from a file
 * @param bot the bot instance
 * @param fname the file path of the command
 */
void loadOneCommand(Bot& bot, const std::filesystem::path fname);

/**
 * @brief loads all commands from a file
 * @param bot the bot instance
 * @param filename the file path of the commands file
 */
void loadCommandsFromFile(Bot& bot, const std::filesystem::path filename);
