#include <BotAddCommand.h>
#include "../CommandModule.h"

struct dynamicCommandModule {
    CommandModule mod;
    bool (*isSupported)();
};

#define DYN_COMMAND_SYM cmd
#define DYN_COMMAND_SYM_STR "cmd"