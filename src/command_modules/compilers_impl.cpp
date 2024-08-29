#include <CompilerPaths.hpp>
#include <StringResManager.hpp>
#include <memory>

#include "TgBotWrapper.hpp"
#include "compiler/Helper.hpp"
#include "compiler/CompilerInTelegram.hpp"

DECLARE_COMMAND_HANDLER(c, bot, message) {
    CompilerInTgForCCpp::Params params;
    const auto intf = std::make_shared<CompilerInTgBotInterface>(bot, message);
    params.exe = kCCompiler;
    params.outfile = "out.c";

    CompilerInTgForCCpp c(intf, params);
    c.run(message);
}

DECLARE_COMMAND_HANDLER(cpp, bot, message) {
    CompilerInTgForCCpp::Params params;
    const auto intf = std::make_shared<CompilerInTgBotInterface>(bot, message);
    params.exe = kCXXCompiler;
    params.outfile = "out.cpp";

    CompilerInTgForCCpp cpp(intf, params);
    cpp.run(message);
}


DECLARE_COMMAND_HANDLER(py, bot, message) {
    CompilerInTgForGeneric::Params params;
    const auto intf = std::make_shared<CompilerInTgBotInterface>(bot, message);
    params.exe = kPythonInterpreter;
    params.outfile = "out.py";

    CompilerInTgForGeneric py(intf, params);
    py.run(message);
}

DYN_COMMAND_FN(name, module) {
    if (name == nullptr) {
        return false;
    }
    const std::string commandName = name;
    module.command = commandName;
    module.isLoaded = true;
    module.flags = CommandModule::Flags::Enforced;
    if (commandName == "c") {
        module.fn = COMMAND_HANDLER_NAME(c);
        module.description = "Run C source code in-chat";
    } else if (commandName == "cpp") {
        module.fn = COMMAND_HANDLER_NAME(cpp);
        module.description = "Run C++ source code in-chat";
    } else if (commandName == "py") {
        module.fn = COMMAND_HANDLER_NAME(py);
        module.description = "Run Python script in-chat";
    } else {
        return false;
    }
    return true;
}
