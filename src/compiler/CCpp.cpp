#include <CompilerInTelegram.h>
#include <ExtArgs.h>
#include <libos/libfs.h>

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
#ifdef __WIN32
    const char aoutname[] = "./a.exe";
#else
    const char aoutname[] = "./a.out";
#endif

    if (verifyAndWriteMessage(message, outfile)) {
        cmd << cmdPrefix << SPACE << outfile;
        if (hasExtArgs(message)) {
            parseExtArgs(message, extraargs);
            appendExtArgs(cmd, extraargs, resultbuf);
        }

        resultbuf << "Compile time:" << std::endl;
        runCommand(message, cmd.str(), resultbuf);
        resultbuf << std::endl;

        if (fileExists(aoutname)) {
            resultbuf << "Run time:\n";
            runCommand(message, aoutname, resultbuf);
            std::filesystem::remove(aoutname);
        }
        onResultReady(message, resultbuf.str());
        std::filesystem::remove(outfile);
    }
}