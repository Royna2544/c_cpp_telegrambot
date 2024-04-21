#include <BotAddCommand.h>
#include "../CommandModule.h"

struct dynamicCommandModule {
    CommandModule mod;
    bool (*isSupported)() = nullptr;
};

#define DYN_COMMAND_SYM cmd
#define DYN_COMMAND_SYM_STR "cmd"