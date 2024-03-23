#pragma once

#include <tgbot/Bot.h>

#include <filesystem>
#include <vector>
#include "InstanceClassBase.hpp"

using TgBot::Bot;
using TgBot::Message;

struct DynamicLibraryHolder {
    DynamicLibraryHolder(void* handle) : handle_(handle){};
    DynamicLibraryHolder(DynamicLibraryHolder&& other);
    ~DynamicLibraryHolder();

   private:
    void* handle_;
};


struct RTCommandLoader : InstanceClassBase<RTCommandLoader> {
    RTCommandLoader(Bot& bot) : bot(bot) {}
    RTCommandLoader() = delete;

    /**
     * @brief loads a single command from a file
     * @param bot the bot instance
     * @param fname the file path of the command
     */
    bool loadOneCommand(const std::filesystem::path fname);

    /**
     * @brief loads all commands from a file
     * @param bot the bot instance
     * @param filename the file path of the commands file
     */
    bool loadCommandsFromFile(std::filesystem::path filename);

    /**
     * @brief returns the path where modules are installed
     *
     * @return std::filesystem::path the path where modules are installed
     */
    static std::filesystem::path getModulesInstallPath();

    static std::filesystem::path getModulesLoadConfPath();

   private:
    static void commandStub(const Bot& bot, const Message::Ptr& message);
    Bot& bot;
    std::vector<DynamicLibraryHolder> libs;
};
