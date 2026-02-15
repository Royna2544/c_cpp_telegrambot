#include <fmt/format.h>

#include <api/StringResLoader.hpp>
#include <filesystem>
#include <libfs.hpp>
#include <system_error>

#include "CompilerInTelegram.hpp"

void CompilerInTgForCCpp::run(MessageExt::Ptr message) {
    std::string extraargs;
    std::stringstream resultbuf;
#ifdef _WIN32
    constexpr const char* aoutname = "./a.exe";
#else
    constexpr const char* aoutname = "./a.out";
#endif

    if (verifyParseWrite(message, extraargs)) {
        std::string cmd = fmt::format("{} {} {}", params.exe.string(),
                                      extraargs, params.outfile.string());

        resultbuf << fmt::format("{}: {}\n", _locale->get(Strings::COMMAND_IS),
                                 cmd);
        runCommand(cmd, resultbuf);
        resultbuf << "\n";

        std::error_code ec;
        if (std::filesystem::exists(aoutname, ec)) {
            resultbuf << _locale->get(Strings::RUN_TIME) << ":\n";
            runCommand(aoutname, resultbuf);
            std::filesystem::remove(aoutname);
        }
        std::filesystem::remove(params.outfile);
        _callback->onResultReady(resultbuf.str());
    }
}