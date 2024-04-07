#include <set>

#include "CommandModule.h"
#include "StringToolsExt.h"
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
    for (const auto &cmd : getLoadedModules()) {
        if (!cmd->isHideDescription()) {
            auto onecommand = std::make_shared<CommandModule>(cmd);
            if (cmd->isEnforced()) onecommand->description += " (Owner)";
            buffer.emplace_back(onecommand);
        }
    }
    bot.getApi().setMyCommands(buffer);
}

void CommandModule::loadCommandModules(Bot &bot) {
    std::set<std::string> commandsName;
    for (const auto &i : getLoadedModules()) {
        commandsName.insert(i->command);
    }
    if (commandsName.size() != getLoadedModules().size()) {
        LOG(WARNING) << "Module names have duplicates";
    }
    for (const auto &i : getLoadedModules()) {
        if (i->fn) {
            if (i->isEnforced())
                bot_AddCommandEnforced(bot, i->command, i->fn);
            else
                bot_AddCommandPermissive(bot, i->command, i->fn);
        } else {
            LOG(ERROR) << "Invalid command module " << i->command
                       << ": No functions provided";
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
        LOG(WARNING) << "Unsupported cmd '" << cmd << "' (compiler)";
        bot_AddCommandEnforced(bot, cmd, NoCompilerCommandStub);
    }
}

void CommandModule::loadCompilerModule(
    Bot &bot, std::initializer_list<CompilerModule *> list) {
    for (auto &module : list) {
        bot_AddCommandEnforcedCompiler(bot, module->command, module->lang,
                                       module->cb);
        getLoadedModules().emplace_back(module);
    }
}
