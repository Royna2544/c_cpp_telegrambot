#include <BotAddCommand.h>
#include <command_modules/CommandModule.h>

struct dynamicCommandModule {
    CommandModule mod;
    bool (*isSupported)();
};

#define DYN_COMMAND_SYM cmd
#define DYN_COMMAND_SYM_STR "cmd"
#define _DECL_DYN_COMMAND(_enforced, _name, _fn, supported) \
    extern "C" {                                            \
    struct dynamicCommandModule DYN_COMMAND_SYM {           \
        .mod = {                                            \
            .enforced = _enforced,                          \
            .name = _name,                                  \
            .fn = _fn,                                      \
        },                                                  \
        .isSupported = supported,                           \
    };                                                      \
    }

#define DECL_DYN_ENFORCED_COMMAND(name, fn, isSupp) _DECL_DYN_COMMAND(true, name, fn, isSupp)
#define DECL_DYN_PERMISSIVE_COMMAND(name, fn, isSupp) _DECL_DYN_COMMAND(false, name, fn, isSupp)
