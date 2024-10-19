#include <CompilerPaths.hpp>
#include <StringResManager.hpp>
#include <api/CommandModule.hpp>
#include <memory>

#include "compiler/CompilerInTelegram.hpp"
#include "compiler/Helper.hpp"

DECLARE_COMMAND_HANDLER(c, bot, message) {
    CompilerInTgForCCpp::Params params;
    auto intf = std::make_unique<CompilerInTgBotInterface>(bot, message);
    params.exe = kCCompiler;
    params.outfile = "out.c";

    CompilerInTgForCCpp c(std::move(intf), std::move(params));
    c.run(message);
}

DECLARE_COMMAND_HANDLER(cpp, bot, message) {
    CompilerInTgForCCpp::Params params;
    auto intf = std::make_unique<CompilerInTgBotInterface>(bot, message);
    params.exe = kCXXCompiler;
    params.outfile = "out.cpp";

    CompilerInTgForCCpp cpp(std::move(intf), std::move(params));
    cpp.run(message);
}

DECLARE_COMMAND_HANDLER(py, bot, message) {
    CompilerInTgForGeneric::Params params;
    auto intf = std::make_unique<CompilerInTgBotInterface>(bot, message);
    params.exe = kPythonInterpreter;
    params.outfile = "out.py";

    CompilerInTgForGeneric py(std::move(intf), std::move(params));
    py.run(message);
}

DYN_COMMAND_FN(commandName, module) {
    module.name = commandName;
    module.flags = CommandModule::Flags::Enforced;
    if (commandName == "c") {
        module.function = COMMAND_HANDLER_NAME(c);
        module.description = "Run C source code in-chat";
    } else if (commandName == "cpp") {
        module.function = COMMAND_HANDLER_NAME(cpp);
        module.description = "Run C++ source code in-chat";
    } else if (commandName == "py") {
        module.function = COMMAND_HANDLER_NAME(py);
        module.description = "Run Python script in-chat";
    } else {
        return false;
    }
    return true;
}
