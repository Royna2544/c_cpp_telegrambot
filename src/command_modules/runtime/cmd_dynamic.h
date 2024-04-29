#include <BotAddCommand.h>

#include <stdexcept>

#define DYN_COMMAND_SYM loadcmd
#define DYN_COMMAND_SYM_STR "loadcmd"

struct unsupported_command_error : public std::runtime_error {
    explicit unsupported_command_error(const std::string& arg)
        : std::runtime_error(arg) {}
};