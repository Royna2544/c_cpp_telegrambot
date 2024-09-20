#include <StringResManager.hpp>
#include <libos/libfs.hpp>

#include "CompilerInTelegram.hpp"
#include "TgBotWrapper.hpp"

void CompilerInTgForCCpp::run(const MessagePtr message) {
    std::string extraargs;
    std::stringstream cmd, resultbuf;
#ifdef WINDOWS_BUILD
    constexpr std::string_view aoutname = "./a.exe";
#else
    constexpr std::string_view aoutname = "./a.out";
#endif

    if (verifyParseWrite(message, extraargs)) {
        cmd << params.exe.string() << SPACE << extraargs << SPACE << params.outfile.string();

        resultbuf << GETSTR_IS(COMPILE_TIME) << std::endl;
        runCommand(cmd.str(), resultbuf);
        resultbuf << std::endl;

        if (FS::exists(aoutname)) {
            resultbuf << GETSTR_IS(RUN_TIME) << std::endl;
            runCommand(aoutname.data(), resultbuf);
            std::filesystem::remove(aoutname);
        }
        std::filesystem::remove(params.outfile);
        _interface->onResultReady(resultbuf.str());
    }
}