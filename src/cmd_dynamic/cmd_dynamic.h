#include <NamespaceImport.h>

struct dynamicCommand {
    bool enforced;
    const char* name;
    void (*fn)(const Bot& bot, const Message::Ptr& message);
};

#define DYN_COMMAND_SYM cmd
#define DYN_COMMAND_SYM_STR "cmd"
#define DECL_DYN_COMMAND(enf, name_, fn_)   \
    extern "C" {                            \
    struct dynamicCommand DYN_COMMAND_SYM { \
        .enforced = enf,                    \
        .name = name_,                      \
        .fn = fn_,                          \
    };                                      \
    }
