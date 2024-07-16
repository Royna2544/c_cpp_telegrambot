#include <StringResManager.hpp>

#include "StringToolsExt.hpp"
#include "TgBotWrapper.hpp"
#include "compiler/CompilerInTelegram.hpp"

using command_callback_compiler_t =
    std::function<void(MessagePtr, const std::filesystem::path &compiler)>;
namespace {

void CModuleCallback(MessagePtr message,
                     const std::filesystem::path &compiler) {
    static CompilerInTgForCCppImpl cCompiler(compiler, "foo.c");
    cCompiler.run(message);
}
void CPPModuleCallback(MessagePtr message,
                       const std::filesystem::path &compiler) {
    static CompilerInTgForCCppImpl cppCompiler(compiler, "foo.cpp");
    cppCompiler.run(message);
}
void PYModuleCallback(MessagePtr message,
                      const std::filesystem::path &compiler) {
    static CompilerInTgForGenericImpl pyCompiler(compiler, "foo.py");
    pyCompiler.run(message);
}
void GOModuleCallback(MessagePtr message,
                      const std::filesystem::path &compiler) {
    static CompilerInTgForGenericImpl goCompiler(compiler, "foo.go");
    goCompiler.run(message);
}
void NoCompilerCommandStub(TgBotApi *wrapper, MessagePtr message) {
    wrapper->sendReplyMessage(message, GETSTR(NOT_SUPPORTED_IN_CURRENT_HOST));
}

void loadCompilerGeneric(CommandModule &module, ProLangs lang,
                         std::string_view name,
                         const command_callback_compiler_t &callback) {
    std::filesystem::path compiler;
    module.description = std::string(name) + " command";
    if (findCompiler(lang, compiler)) {
        module.fn = [compiler, callback](auto *, MessagePtr message) {
            callback(message, compiler);
        };
        module.description += ", ";
        module.description += compiler.make_preferred().string();
    } else {
        LOG(WARNING) << "Unsupported cmd " << SingleQuoted(name)
                     << " (compiler)";
        module.fn = NoCompilerCommandStub;
    }
}

}  // namespace

DYN_COMMAND_FN(name, module) {
    if (name == nullptr) {
        return false;
    }
    std::string commandName = name;
    module.command = commandName;
    module.isLoaded = true;
    module.flags = CommandModule::Flags::Enforced;
    if (commandName == "c") {
        loadCompilerGeneric(module, ProLangs::C, commandName, CModuleCallback);

    } else if (commandName == "cpp") {
        loadCompilerGeneric(module, ProLangs::CXX, commandName,
                            CPPModuleCallback);

    } else if (commandName == "py") {
        loadCompilerGeneric(module, ProLangs::PYTHON, commandName,
                            PYModuleCallback);

    } else if (commandName == "go") {
        loadCompilerGeneric(module, ProLangs::GO, commandName,
                            GOModuleCallback);
    }
    return true;
}
