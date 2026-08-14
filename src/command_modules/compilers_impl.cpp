#include <fmt/format.h>

#include <CompilerPaths.hpp>
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
    auto ownedMessage = std::make_shared<MessageExt>(*message);
    if (!api->submitCommandWork(
            "c", TgBotApi::WorkClass::Process,
            [api, res,
             ownedMessage = std::move(ownedMessage)](std::stop_token stop) {
                CompilerInTgForCCpp::Params params;
                params.exe = kCCompiler;
                params.outfile = makeCompilerTempPath(ownedMessage.get(), ".c");
                auto intf = std::make_unique<CompilerInTgBotInterface>(
                    api, res, ownedMessage.get());
                CompilerInTgForCCpp compiler(std::move(intf), res,
                                             std::move(params));
                compiler.run(ownedMessage.get(), stop);
            })) {
        api->sendReplyMessage(message->message(),
                              "The process queue is full. Please retry later.");
    }
}

DECLARE_COMMAND_HANDLER(cpp) {
    auto ownedMessage = std::make_shared<MessageExt>(*message);
    if (!api->submitCommandWork(
            "cpp", TgBotApi::WorkClass::Process,
            [api, res,
             ownedMessage = std::move(ownedMessage)](std::stop_token stop) {
                CompilerInTgForCCpp::Params params;
                params.exe = kCXXCompiler;
                params.outfile =
                    makeCompilerTempPath(ownedMessage.get(), ".cpp");
                auto intf = std::make_unique<CompilerInTgBotInterface>(
                    api, res, ownedMessage.get());
                CompilerInTgForCCpp compiler(std::move(intf), res,
                                             std::move(params));
                compiler.run(ownedMessage.get(), stop);
            })) {
        api->sendReplyMessage(message->message(),
                              "The process queue is full. Please retry later.");
    }
}

DECLARE_COMMAND_HANDLER(py) {
    auto ownedMessage = std::make_shared<MessageExt>(*message);
    if (!api->submitCommandWork(
            "py", TgBotApi::WorkClass::Process,
            [api, res,
             ownedMessage = std::move(ownedMessage)](std::stop_token stop) {
                CompilerInTgForGeneric::Params params;
                params.exe = kPythonInterpreter;
                params.outfile =
                    makeCompilerTempPath(ownedMessage.get(), ".py");
                auto intf = std::make_unique<CompilerInTgBotInterface>(
                    api, res, ownedMessage.get());
                CompilerInTgForGeneric interpreter(std::move(intf), res,
                                                   std::move(params));
                interpreter.run(ownedMessage.get(), stop);
            })) {
        api->sendReplyMessage(message->message(),
                              "The process queue is full. Please retry later.");
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::OwnerOnly,
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
