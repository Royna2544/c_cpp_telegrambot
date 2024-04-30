#pragma once

#include <BotAddCommand.h>
#include <tgbot/types/BotCommand.h>

#include <functional>
#include <initcalls/BotInitcall.hpp>

struct CommandModule : TgBot::BotCommand {
    enum Flags { None = 0, Enforced = 1 << 0, HideDescription = 1 << 1 };
    command_callback_t fn;
    unsigned int flags{};

    [[nodiscard]] constexpr bool isEnforced() const { return (flags & Enforced) != 0; }
    [[nodiscard]] bool isHideDescription() const { return (flags & HideDescription) != 0; }
};

struct CommandModuleManager : BotInitCall {
    static std::string getLoadedModulesString();
    static inline std::vector<CommandModule> loadedModules;

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
    const CStringLifetime getInitCallName() const override {
        return "Load/update default modules";
    }
};

using command_loader_function_t = std::function<void(CommandModule &)>;
