#pragma once

#include <BotAddCommand.h>

struct CommandModule {
    bool enforced;
    const char *name;
    command_callback_t fn;
};