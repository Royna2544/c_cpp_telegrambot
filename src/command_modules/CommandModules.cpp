
#include <functional>

#include "BotAddCommand.h"
#include "CommandModule.h"
#include "StringToolsExt.h"
#include "compiler/CompilerInTelegram.h"

using command_callback_compiler_t = std::function<void(
    const Bot &bot, const Message::Ptr &message, const std::string &compiler)>;
namespace {

void CModuleCallback(const Bot &bot, const Message::Ptr &message,
                     const std::string &compiler) {
    static CompilerInTgForCCppImpl cCompiler(bot, compiler, "foo.c");
    cCompiler.run(message);
}
void CPPModuleCallback(const Bot &bot, const Message::Ptr &message,
                       const std::string &compiler) {
    static CompilerInTgForCCppImpl cppCompiler(bot, compiler, "foo.cpp");
    cppCompiler.run(message);
}
void PYModuleCallback(const Bot &bot, const Message::Ptr &message,
                      const std::string &compiler) {
    static CompilerInTgForGenericImpl pyCompiler(bot, compiler, "foo.py");
    pyCompiler.run(message);
}
void GOModuleCallback(const Bot &bot, const Message::Ptr &message,
                      const std::string &compiler) {
    static CompilerInTgForGenericImpl goCompiler(bot, compiler, "foo.go");
    goCompiler.run(message);
}
void NoCompilerCommandStub(const Bot &bot, const Message::Ptr &message) {
    bot_sendReplyMessage(bot, message, "Not supported in current host");
}
void loadCompilerGeneric(CommandModule &module, ProgrammingLangs lang,
                         std::string_view name,
                         const command_callback_compiler_t &callback) {
    std::string compiler;
    module.flags = CommandModule::Flags::Enforced;
    module.command = name;
    module.description = std::string(name) + " command";
    if (findCompiler(lang, compiler)) {
        module.fn = [compiler, callback](const Bot &bot,
                                          const Message::Ptr &message) {
            callback(bot, message, compiler);
        };
        module.description += ", " + compiler;
    } else {
        LOG(WARNING) << "Unsupported cmd '" << name << "' (compiler)";
        module.fn = NoCompilerCommandStub;
    }
}

}  // namespace

void loadcmd_c(CommandModule &module) {
    loadCompilerGeneric(module, ProgrammingLangs::C, "c", CModuleCallback);
}
void loadcmd_cpp(CommandModule &module) {
    loadCompilerGeneric(module, ProgrammingLangs::CXX, "cpp",
                        CPPModuleCallback);
}
void loadcmd_python(CommandModule &module) {
    loadCompilerGeneric(module, ProgrammingLangs::PYTHON, "python",
                        PYModuleCallback);
}
void loadcmd_go(CommandModule &module) {
    loadCompilerGeneric(module, ProgrammingLangs::GO, "go", GOModuleCallback);
}

std::string CommandModule::getLoadedModulesString() {
    std::stringstream ss;
    std::string outbuf;

    for (auto module : loadedModules) {
        ss << module.command << " ";
    }
    outbuf = ss.str();
    TrimStr(outbuf);
    return outbuf;
}

void CommandModule::updateBotCommands(const Bot &bot) {
    std::vector<BotCommand::Ptr> buffer;
    for (const auto &cmd : loadedModules) {
        if (!cmd.isHideDescription()) {
            auto onecommand = std::make_shared<CommandModule>(cmd);
            if (cmd.isEnforced()) {
                onecommand->description += " (Owner)";
            }
            buffer.emplace_back(onecommand);
        }
    }
    bot.getApi().setMyCommands(buffer);
}
