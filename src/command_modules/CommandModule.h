#pragma once

#include <BotAddCommand.h>
#include <tgbot/types/BotCommand.h>

struct CommandModule : TgBot::BotCommand {
    enum Flags { None = 0, Enforced = 1 << 0, HideDescription = 1 << 1 };
    command_callback_t fn;
    command_callback_modbot_t mfn;
    int flags;

    explicit CommandModule(const std::string &name,
                           const std::string &description, int flags,
                           command_callback_t fn,
                           command_callback_modbot_t mod = {})
        : fn(fn), mfn(mod), flags(flags) {
        this->command = name;
        this->description = description;
        if (mod) {
            ASSERT(
                isEnforced(),
                "Modbot callback must be specified for enforced commands only");
        }
    }
    CommandModule() = default;
    CommandModule(const CommandModule *other) { *this = *other; }
    constexpr bool isEnforced() const { return flags & Enforced; }
    bool isHideDescription() const { return flags & HideDescription; }
};