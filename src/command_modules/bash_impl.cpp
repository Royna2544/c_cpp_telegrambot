#include <api/CommandModule.hpp>

#include "compiler/CompilerInTelegram.hpp"
#include "compiler/Helper.hpp"

namespace {

DECLARE_COMMAND_HANDLER(bash) {
    auto helper = std::make_unique<CompilerInTgBotInterface>(api, res, message);
    CompilerInTgForBash bash(std::move(helper), res, false);
    bash.run(message);
}
DECLARE_COMMAND_HANDLER(ubash) {
    auto helper = std::make_unique<CompilerInTgBotInterface>(api, res, message);
    CompilerInTgForBash ubash(std::move(helper), res, true);
    ubash.run(message);
}

}  // namespace


extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::Enforced,
#ifdef cmd_bash_EXPORTS
    .name = "bash",
    .description = "Run bash commands",
    .function = COMMAND_HANDLER_NAME(bash),
#endif
#ifdef cmd_ubash_EXPORTS
    .name = "ubash",
    .description = "Run bash commands w/o timeout",
    .function = COMMAND_HANDLER_NAME(ubash),
#endif
    .valid_args = {}
};
