#include <fmt/format.h>

#include <filesystem>
#include <libfs.hpp>
#include <system_error>

#include "CompilerInTelegram.hpp"
#include <api/StringResLoader.hpp>

void CompilerInTgForCCpp::run(MessageExt::Ptr message) {
    std::string extraargs;
    std::stringstream cmd, resultbuf;
#ifdef _WIN32
    constexpr std::string_view aoutname = "./a.exe";
#else
    constexpr std::string_view aoutname = "./a.out";
#endif

    if (verifyParseWrite(message, extraargs)) {
        cmd << params.exe.string() << SPACE << extraargs << SPACE
            << params.outfile.string();

        resultbuf << fmt::format("{}: {}\n", _locale->get(Strings::COMMAND_IS), cmd.str());
        runCommand(cmd.str(), resultbuf);
        resultbuf << std::endl;

        std::error_code ec;
        if (std::filesystem::exists(aoutname, ec)) {
            resultbuf << _locale->get(Strings::RUN_TIME) << ":\n";
            runCommand(aoutname.data(), resultbuf);
            std::filesystem::remove(aoutname);
        }
        std::filesystem::remove(params.outfile);
        _interface->onResultReady(resultbuf.str());
    }
}