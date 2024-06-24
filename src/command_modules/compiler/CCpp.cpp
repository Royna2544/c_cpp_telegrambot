#include "CompilerInTelegram.h"
#include <ExtArgs.h>
#include <libos/libfs.hpp>
#include <StringResManager.hpp>

void CompilerInTgForCCppImpl::onResultReady(const Message::Ptr& who,
                                            const std::string& text) {
    CompilerInTgHelper::onResultReady(_bot, who, text);
}
void CompilerInTgForCCppImpl::onFailed(const Message::Ptr& who,
                                       const ErrorType e) {
    CompilerInTgHelper::onFailed(_bot, who, e);
}

void CompilerInTgForCCpp::run(const Message::Ptr& message) {
    std::string extraargs;
    std::stringstream cmd, resultbuf;
#ifdef WINDOWS_BUILD
    const char aoutname[] = "./a.exe";
#else
    const char aoutname[] = "./a.out";
#endif

    if (verifyParseWrite(message, extraargs)) {
        cmd << cmdPrefix.string() << SPACE << extraargs << SPACE << outfile;

        resultbuf << GETSTR_IS(COMPILE_TIME) << std::endl;
        runCommand(message, cmd.str(), resultbuf);
        resultbuf << std::endl;

        if (FS::exists(aoutname)) {
            resultbuf << GETSTR_IS(RUN_TIME) << std::endl;
            runCommand(message, aoutname, resultbuf);
            std::filesystem::remove(aoutname);
        }
        onResultReady(message, resultbuf.str());
        std::filesystem::remove(outfile);
    }
}