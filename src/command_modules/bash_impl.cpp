#include <StringResManager.hpp>

#include "TgBotWrapper.hpp"
#include "compiler/CompilerInTelegram.hpp"
#include "compiler/Helper.hpp"

namespace {

DECLARE_COMMAND_HANDLER(bash, wrapper, message) {
    const auto helper =
        std::make_shared<CompilerInTgBotInterface>(wrapper, message);
    CompilerInTgForBash bash(helper);
    bash.run(message);
}
DECLARE_COMMAND_HANDLER(ubash, wrapper, message) {
    const auto helper =
        std::make_shared<CompilerInTgBotInterface>(wrapper, message);
    CompilerInTgForBash ubash(helper);
    ubash.allowHang(true);
    ubash.run(message);
}

}  // namespace

DYN_COMMAND_FN(name, module) {
    module.command = name;
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
