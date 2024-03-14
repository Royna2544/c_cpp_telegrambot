
#include <BotAddCommand.h>

#include "CommandModule.h"
#include "compiler/CompilerInTelegram.h"

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

void setupCompilerInTg(Bot &bot) {
    static CompilerModule moduleC(
        "c", ProgrammingLangs::C,
        [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
            static CompilerInTgForCCppImpl cCompiler(bot, compiler, "foo.c");
            cCompiler.run(message);
        });
    static CompilerModule moduleCpp(
        "cpp", ProgrammingLangs::CXX,
        [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
            static CompilerInTgForCCppImpl cxxCompiler(bot, compiler,
                                                       "foo.cpp");
            cxxCompiler.run(message);
        });
    static CompilerModule modulePython(
        "python", ProgrammingLangs::PYTHON,
        [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
            static CompilerInTgForCCppImpl cxxCompiler(bot, compiler, "foo.py");
            cxxCompiler.run(message);
        });
    static CompilerModule moduleGolang(
        "go", ProgrammingLangs::GO,
        [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
            static CompilerInTgForCCppImpl cxxCompiler(bot, compiler + " run",
                                                       "foo.go");
            cxxCompiler.run(message);
        });
    for (const auto &module :
         {moduleC, moduleCpp, modulePython, moduleGolang}) {
        bot_AddCommandEnforcedCompiler(bot, module.command, module.lang,
                                       module.cb);
    }
}
