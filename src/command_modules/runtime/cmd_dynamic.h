#include <BotAddCommand.h>
#include <command_modules/CommandModule.h>

struct dynamicCommandModule {
    CommandModule mod;
    bool (*isSupported)();
};

#define DYN_COMMAND_SYM cmd
#define DYN_COMMAND_SYM_STR "cmd"