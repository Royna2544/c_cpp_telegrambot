#pragma once

#include <tgbot/Bot.h>

#include <filesystem>
#include <vector>

#include "InstanceClassBase.hpp"
#include "initcalls/BotInitcall.hpp"

using TgBot::Bot;
using TgBot::Message;

struct DynamicLibraryHolder {
    explicit DynamicLibraryHolder(void* handle) : handle_(handle){};
    DynamicLibraryHolder(DynamicLibraryHolder&& other);
    ~DynamicLibraryHolder();

   private:
    void* handle_;
};

struct RTCommandLoader : public InstanceClassBase<RTCommandLoader>,
                         BotInitCall {
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
     * @brief returns the path where modules load conf is installed
     *
     * @return std::filesystem::path the path where modules load conf is
     * installed
     */
    static std::filesystem::path getModulesLoadConfPath();

    void doInitCall(Bot& bot) override {
        loadCommandsFromFile(getModulesLoadConfPath());
    }
    const char * getInitCallName() const override {
        return "Load commands from file";
    }

   private:
    static void commandStub(const Bot& bot, const Message::Ptr& message);
    Bot& bot;
    std::vector<std::shared_ptr<DynamicLibraryHolder>> libs;
};
