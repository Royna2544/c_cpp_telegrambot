#include <StringResManager.hpp>

#include <api/CommandModule.hpp>
#include "compiler/CompilerInTelegram.hpp"
#include "compiler/Helper.hpp"

namespace {

DECLARE_COMMAND_HANDLER(bash, wrapper, message) {
    auto helper =
        std::make_unique<CompilerInTgBotInterface>(wrapper, message);
    CompilerInTgForBash bash(std::move(helper), false);
    bash.run(message);
}
DECLARE_COMMAND_HANDLER(ubash, wrapper, message) {
    auto helper =
        std::make_unique<CompilerInTgBotInterface>(wrapper, message);
    CompilerInTgForBash ubash(std::move(helper), true);
    ubash.run(message);
}

}  // namespace

DYN_COMMAND_FN(name, module) {
    module.name = name;
    module.flags = CommandModule::Flags::Enforced;
    if (name == "bash") {
        module.description = GETSTR(BASH_CMD_DESC);
        module.function = COMMAND_HANDLER_NAME(bash);
    } else if (name == "ubash") {
        module.description = GETSTR(UBASH_CMD_DESC);
        module.function = COMMAND_HANDLER_NAME(ubash);
    }
    return true;
}
