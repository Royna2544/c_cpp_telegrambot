#pragma once

#include <BotAddCommand.h>
#include <tgbot/types/BotCommand.h>

#include <initcalls/BotInitcall.hpp>
#include <initializer_list>

struct CompilerModule;
struct CommandModule : TgBot::BotCommand, BotInitCall {
    enum Flags { None = 0, Enforced = 1 << 0, HideDescription = 1 << 1 };
    command_callback_t fn;
    int flags;

    explicit CommandModule(const std::string &name,
                           const std::string &description, int flags,
                           command_callback_t fn)
        : fn(fn), flags(flags) {
        this->command = name;
        this->description = description;
    }
    CommandModule() = default;
    CommandModule(const CommandModule *other) { *this = *other; }
    constexpr bool isEnforced() const { return flags & Enforced; }
    bool isHideDescription() const { return flags & HideDescription; }
    static std::string getLoadedModulesString();

    /**
     * Returns a string containing the names of all loaded modules, separated by
     * spaces.
     */
    static std::vector<CommandModule *> getLoadedModules();

    /**
     * Adds a list of CompilerModules to the bot.
     * @param bot The Bot instance to add the modules to.
     * @param list The list of CompilerModules to add.
     */
    static void loadCompilerModule(
        Bot &bot, std::initializer_list<CompilerModule *> list);

    /**
     * Adds default CommandModules to the bot.
     * @param bot The Bot instance to add the modules to.
     */
    static void loadCommandModules(Bot &bot);

    /**
     * Updates the list of bot commands based on the currently loaded
     * CommandModules.
     * @param bot The Bot instance to update the commands for.
     */
    static void updateBotCommands(const Bot &bot);

    void doInitCall(Bot &bot) override {
        loadCommandModules(bot);
        updateBotCommands(bot);
    }
    const char *getInitCallName() const override {
        return "Load/update default modules";
    }
};
