#include "CommandModule.h"
#include "cmds.gen.h"
#include "compiler/CompilerInTelegram.h"

/**
 * command_callback_compiler_t - callback function for a compiler related
 * command handler Passes a Bot reference object and callback message pointer,
 * and additionally compiler exe path as string
 */
using command_callback_compiler_t = std::function<void(
    const Bot &, const Message::Ptr &, const std::string &compiler)>;

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

struct CompilerModule : CommandModule {
    ProgrammingLangs lang;
    command_callback_compiler_t cb;
    explicit CompilerModule(std::string cmd, ProgrammingLangs lang,
                            command_callback_compiler_t cb)
        : lang(lang), cb(cb) {
        flags = Enforced | HideDescription;
        command = cmd;
    }
};

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

void setupCompilerInTg(Bot &bot) {
    static std::vector<CompilerModule *> modules = {
        &moduleC, &moduleCpp, &modulePython, &moduleGolang};

    for (auto &module : modules) {
        bot_AddCommandEnforcedCompiler(bot, module->command, module->lang,
                                       module->cb);
        gCmdModules.emplace_back(module);
    }
}
