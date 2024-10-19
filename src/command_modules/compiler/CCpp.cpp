#include <StringResManager.hpp>
#include <libos/libfs.hpp>

#include "CompilerInTelegram.hpp"

void CompilerInTgForCCpp::run(MessageExt::Ptr message) {
    std::string extraargs;
    std::stringstream cmd, resultbuf;
#ifdef WINDOWS_BUILD
    constexpr std::string_view aoutname = "./a.exe";
#else
    constexpr std::string_view aoutname = "./a.out";
#endif

    if (verifyParseWrite(message, extraargs)) {
        cmd << params.exe.string() << SPACE << extraargs << SPACE << params.outfile.string();

        resultbuf << GETSTR(COMPILE_TIME) << ":\n";
        runCommand(cmd.str(), resultbuf);
        resultbuf << std::endl;

        if (FS::exists(aoutname)) {
            resultbuf << GETSTR(RUN_TIME) << ":\n";
            runCommand(aoutname.data(), resultbuf);
            std::filesystem::remove(aoutname);
        }
        std::filesystem::remove(params.outfile);
        _interface->onResultReady(resultbuf.str());
    }
}