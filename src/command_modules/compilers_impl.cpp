#include <CompilerPaths.hpp>
#include <fmt/format.h>

#include <api/CommandModule.hpp>
#include <filesystem>
#include <memory>
#include <system_error>

#include "compiler/CompilerInTelegram.hpp"
#include "compiler/Helper.hpp"

namespace {
std::filesystem::path makeCompilerTempPath(MessageExt::Ptr message,
                                           std::string_view extension) {
    std::error_code ec;
    auto dir = std::filesystem::temp_directory_path(ec);
    if (ec) {
        dir = ".";
    }
    return dir / fmt::format("glider_compile_{}_{}{}",
                             message->get<MessageAttrs::Chat>()->id,
                             message->get<MessageAttrs::MessageId>(),
                             extension);
}
}  // namespace

DECLARE_COMMAND_HANDLER(c) {
    CompilerInTgForCCpp::Params params;
    auto intf = std::make_unique<CompilerInTgBotInterface>(api, res, message);
    params.exe = kCCompiler;
    params.outfile = makeCompilerTempPath(message, ".c");

    CompilerInTgForCCpp c(std::move(intf), res, std::move(params));
    c.run(message);
}

DECLARE_COMMAND_HANDLER(cpp) {
    CompilerInTgForCCpp::Params params;
    auto intf = std::make_unique<CompilerInTgBotInterface>(api, res, message);
    params.exe = kCXXCompiler;
    params.outfile = makeCompilerTempPath(message, ".cpp");

    CompilerInTgForCCpp cpp(std::move(intf), res, std::move(params));
    cpp.run(message);
}

DECLARE_COMMAND_HANDLER(py) {
    CompilerInTgForGeneric::Params params;
    auto intf = std::make_unique<CompilerInTgBotInterface>(api, res, message);
    params.exe = kPythonInterpreter;
    params.outfile = makeCompilerTempPath(message, ".py");

    CompilerInTgForGeneric py(std::move(intf), res, std::move(params));
    py.run(message);
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
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
