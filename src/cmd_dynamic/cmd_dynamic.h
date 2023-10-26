#include <NamespaceImport.h>
#include <BotAddCommand.h>

struct dynamicCommand {
    bool enforced;
    const char* name;
    command_callback_t fn;
    bool (*isSupported)();
};

#define DYN_COMMAND_SYM cmd
#define DYN_COMMAND_SYM_STR "cmd"
#define _DECL_DYN_COMMAND(enf, name_, fn_, isSupp)   \
    extern "C" {                            \
    struct dynamicCommand DYN_COMMAND_SYM { \
        .enforced = enf,                    \
        .name = name_,                      \
        .fn = fn_,                          \
	.isSupported = isSupp,		    \
    };                                      \
    }

#define DECL_DYN_ENFORCED_COMMAND(name, fn, isSupp) _DECL_DYN_COMMAND(true, name, fn, isSupp)
#define DECL_DYN_PERMISSIVE_COMMAND(name, fn, isSupp) _DECL_DYN_COMMAND(false, name, fn, isSupp)
