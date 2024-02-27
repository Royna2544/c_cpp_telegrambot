#pragma once

#include <BotAddCommand.h>
#include <string>

struct CommandModule {
    bool enforced;
    std::string name;
    command_callback_t fn;
};