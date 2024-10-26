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

DYN_COMMAND_FN(name, module) {
    module.name = name;
    module.flags = CommandModule::Flags::Enforced;
    if (name == "bash") {
        module.description = "Run bash commands";
        module.function = COMMAND_HANDLER_NAME(bash);
    } else if (name == "ubash") {
        module.description = "Run bash commands w/o timeout";
        module.function = COMMAND_HANDLER_NAME(ubash);
    }
    return true;
}
