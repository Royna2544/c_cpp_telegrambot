#include <BotReplyMessage.h>

#include <boost/algorithm/string/replace.hpp>
#include <filesystem>
#include <libos/libfs.hpp>
#include <string>

#include "CommandModule.h"
#include "RTCommandLoader.h"
#include "command_modules/compiler/CompilerInTelegram.h"

void RTLoadCommandFn(Bot& bot, const Message::Ptr message) {
    static int count = 0;
    std::string compiler;
    if (!findCompiler(ProgrammingLangs::CXX, compiler)) {
        bot_sendReplyMessage(bot, message, "No CXX compiler found!");
        return;
    }
    auto p = FS::getPathForType(FS::PathType::MODULES_INSTALLED) /
             ("libdlload_" + std::to_string(count));
    auto libcmdmod =
        FS::getPathForType(FS::PathType::BUILD_ROOT) / "libTgBotCommandModules";
    auto libtg = FS::getPathForType(FS::PathType::BUILD_ROOT) / "libTgBot";
    FS::appendDylibExtension(p);
    compiler += " -o " + p.string() +
                " {libcmdmod} {libtg} -I{src}/src/include "
                "-I{src}/lib/include -I{src}/src"
                " -include {src}/src/command_modules/runtime/cmd_dynamic.h "
                "-std=c++20 -shared";
    boost::replace_all(compiler, "{src}",
                       FS::getPathForType(FS::PathType::GIT_ROOT).string());
    boost::replace_all(compiler, "{libcmdmod}",
                       FS::appendDylibExtension(libcmdmod).string());
    boost::replace_all(compiler, "{libtg}",
                       FS::appendDylibExtension(libtg).string());
    CompilerInTgForCCppImpl impl(bot, compiler, "dltmp.cc");
    std::filesystem::remove(p);
    impl.run(message);
    if (FS::exists(p)) {
        if (RTCommandLoader::getInstance().loadOneCommand(p)) {
            bot_sendReplyMessage(bot, message, "Command loaded!");
            count++;
        } else {
            bot_sendReplyMessage(bot, message, "Failed to load command!");
        }
    }
}

void loadcmd_rtload(CommandModule& module) {
    module.command = "rtload";
    module.description = "Runtime load command";
    module.flags = CommandModule::Flags::None;
    module.fn = RTLoadCommandFn;
}