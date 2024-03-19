#include "CommandModule.h"
#include "StringToolsExt.h"
#include "gen/cmds.gen.h"
#include "compiler/CompilerInTelegram.h"

static void CModuleCallback(const Bot &bot, const Message::Ptr &message,
                            std::string compiler) {
    static CompilerInTgForCCppImpl cCompiler(bot, compiler, "foo.c");
    cCompiler.run(message);
}
static void CPPModuleCallback(const Bot &bot, const Message::Ptr &message,
                              std::string compiler) {
    static CompilerInTgForCCppImpl cppCompiler(bot, compiler, "foo.cpp");
    cppCompiler.run(message);
}
static void PYModuleCallback(const Bot &bot, const Message::Ptr &message,
                             std::string compiler) {
    static CompilerInTgForGenericImpl pyCompiler(bot, compiler, "foo.py");
    pyCompiler.run(message);
}
static void GOModuleCallback(const Bot &bot, const Message::Ptr &message,
                             std::string compiler) {
    static CompilerInTgForGenericImpl goCompiler(bot, compiler, "foo.go");
    goCompiler.run(message);
}

static CompilerModule moduleC("c", ProgrammingLangs::C, CModuleCallback);
static CompilerModule moduleCpp("cpp", ProgrammingLangs::CXX,
                                CPPModuleCallback);
static CompilerModule modulePython("python", ProgrammingLangs::PYTHON,
                                   PYModuleCallback);
static CompilerModule moduleGolang("go", ProgrammingLangs::GO,
                                   GOModuleCallback);

std::vector<CommandModule *> CommandModule::getLoadedModules() {
    return gCmdModules;
}

std::string CommandModule::getLoadedModulesString() {
    std::stringstream ss;
    std::string outbuf;

    for (auto module : getLoadedModules()) {
        ss << module->command << " ";
    }
    outbuf = ss.str();
    TrimStr(outbuf);
    return outbuf;
}

void CommandModule::updateBotCommands(const Bot &bot) {
    std::vector<BotCommand::Ptr> buffer;
    for (const auto &cmd : gCmdModules) {
        if (!cmd->isHideDescription()) {
            auto onecommand = std::make_shared<CommandModule>(cmd);
            if (cmd->isEnforced()) onecommand->description += " (Owner)";
            buffer.emplace_back(onecommand);
        }
    }
    bot.getApi().setMyCommands(buffer);
}

void CommandModule::loadCommandModules(Bot &bot) {
    for (const auto &i : gCmdModules) {
        if (i->fn) {
            if (i->isEnforced())
                bot_AddCommandEnforced(bot, i->command, i->fn);
            else
                bot_AddCommandPermissive(bot, i->command, i->fn);
        } else if (i->mfn) {
            if (i->isEnforced())
                bot_AddCommandEnforcedMod(bot, i->command, i->mfn);
        } else {
            LOG(LogLevel::ERROR,
                "Invalid command module %s: No functions provided",
                i->command.c_str());
        }
    }
    loadCompilerModule(bot,
                       {&moduleC, &moduleCpp, &modulePython, &moduleGolang});
}

static void NoCompilerCommandStub(const Bot &bot, const Message::Ptr &message) {
    bot_sendReplyMessage(bot, message, "Not supported in current host");
}

/**
 * bot_AddCommandEnforcedCompiler - Add a bot command specialized for compiler
 *
 * @param bot Bot object
 * @param cmd The command name in string
 * @param lang The compiler type, will be passed to findCompiler() function in
 * libutils.cpp
 * @param cb callback invoked on messages with matching cmd
 * @see Authorization.h
 */
void bot_AddCommandEnforcedCompiler(Bot &bot, const std::string &cmd,
                                    ProgrammingLangs lang,
                                    command_callback_compiler_t cb) {
    std::string compiler;
    if (findCompiler(lang, compiler)) {
        bot_AddCommandEnforced(bot, cmd,
                               std::bind(cb, std::placeholders::_1,
                                         std::placeholders::_2, compiler));
    } else {
        LOG(LogLevel::WARNING, "Unsupported cmd '%s' (compiler)", cmd.c_str());
        bot_AddCommandEnforced(bot, cmd, NoCompilerCommandStub);
    }
}

void CommandModule::loadCompilerModule(
    Bot &bot, std::initializer_list<CompilerModule *> list) {
    for (auto &module : list) {
        bot_AddCommandEnforcedCompiler(bot, module->command, module->lang,
                                       module->cb);
        gCmdModules.emplace_back(module);
    }
}
