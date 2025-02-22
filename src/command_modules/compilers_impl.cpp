#include <CompilerPaths.hpp>
#include <api/CommandModule.hpp>
#include <memory>

#include "compiler/CompilerInTelegram.hpp"
#include "compiler/Helper.hpp"

DECLARE_COMMAND_HANDLER(c) {
    CompilerInTgForCCpp::Params params;
    auto intf = std::make_unique<CompilerInTgBotInterface>(api, res, message);
    params.exe = kCCompiler;
    params.outfile = "out.c";

    CompilerInTgForCCpp c(std::move(intf), res, std::move(params));
    c.run(message);
}

DECLARE_COMMAND_HANDLER(cpp) {
    CompilerInTgForCCpp::Params params;
    auto intf = std::make_unique<CompilerInTgBotInterface>(api, res, message);
    params.exe = kCXXCompiler;
    params.outfile = "out.cpp";

    CompilerInTgForCCpp cpp(std::move(intf), res, std::move(params));
    cpp.run(message);
}

DECLARE_COMMAND_HANDLER(py) {
    CompilerInTgForGeneric::Params params;
    auto intf = std::make_unique<CompilerInTgBotInterface>(api, res, message);
    params.exe = kPythonInterpreter;
    params.outfile = "out.py";

    CompilerInTgForGeneric py(std::move(intf), res, std::move(params));
    py.run(message);
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::Enforced,
#ifdef cmd_c_EXPORTS
    .name = "c",
    .description = "Run C source code in-chat",
    .function = COMMAND_HANDLER_NAME(c),
#endif
#ifdef cmd_cpp_EXPORTS
    .name = "cpp",
    .description = "Run C++ source code in-chat",
    .function = COMMAND_HANDLER_NAME(cpp),
#endif
#ifdef cmd_py_EXPORTS
    .name = "py",
    .description = "Run Python script in-chat",
    .function = COMMAND_HANDLER_NAME(py),
#endif
    .valid_args = {}};