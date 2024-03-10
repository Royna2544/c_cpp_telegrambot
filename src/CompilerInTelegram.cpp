#include <Authorization.h>
#include <BotReplyMessage.h>
#include <CompilerInTelegram.h>
#include <ConfigManager.h>
#include <EnumArrayHelpers.h>
#include <ExtArgs.h>
#include <libos/libfs.h>
#include <Logging.h>
#include <StringToolsExt.h>
#include <random/RandomNumberGenerator.h>

#include <boost/algorithm/string/replace.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>

#include "popen_wdt/popen_wdt.h"

using std::chrono_literals::operator""ms;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

static const char SPACE = ' ';
static const char EMPTY[] = "(empty)";

static bool verifyMessage(const Bot &bot, const Message::Ptr &message) {
    if (message->replyToMessage && !message->replyToMessage->text.empty()) {
        return true;
    }
    bot_sendReplyMessage(bot, message, "Reply to a code to compile");
    return false;
}

static bool writeMessageToFile(const Bot &bot, const Message::Ptr &message,
                               const std::string &filename) {
    std::ofstream file(filename);
    if (file.fail()) {
        bot_sendReplyMessage(bot, message, "Failed to open file");
        return false;
    }
    file << message->replyToMessage->text;
    file.close();
    return true;
}

static void addExtArgs(std::stringstream &cmd, std::string &extraargs,
                       std::string &res) {
    if (!extraargs.empty()) {
        auto idx = extraargs.find_first_of("\n\r\t");
        if (idx != std::string::npos) extraargs.erase(idx);
        cmd << SPACE << extraargs;
        res += "cmd: \"";
        res += cmd.str();
        res += "\"\n";
    }
}

static void runCommand(const Bot &bot, const Message::Ptr &message,
                       std::string cmd, std::string &res, bool use_wdt = true) {
    bool hasmore = false, watchdog_bitten = false;
    int count = 0, unique_id = 0;;
    char buf[BASH_READ_BUF] = {};
    std::error_code ec;

#ifdef LOCALE_EN_US
    static std::once_flag once;
    std::call_once(once, [] {
        setlocale_enus_once();
    });
#endif
    unique_id = genRandomNumber(100);
    boost::replace_all(cmd, std::string(1, '"'), "\\\"");

#define ID_LOG_I(id, fmt, ...) LOG_I("[ID %d] " fmt, id, ##__VA_ARGS__)
    ID_LOG_I(unique_id, "%s: +++", __func__);
    ID_LOG_I(unique_id, "Command: %s", cmd.c_str());
    auto start = high_resolution_clock::now();
    auto fp = popen_watchdog(cmd.c_str(), use_wdt ? &watchdog_bitten : nullptr);

    if (!fp) {
        bot_sendReplyMessage(bot, message, "Failed to popen()");
        return;
    }
    res.reserve(BASH_MAX_BUF);
    while (fgets(buf, sizeof(buf), fp)) {
        if (res.size() < BASH_MAX_BUF) {
            res += buf;
        } else {
            hasmore = true;
        }
        count++;
        std::this_thread::sleep_for(50ms);
    }
    auto srcroot = std::filesystem::current_path(ec);
    if (!ec) {
        srcroot.make_preferred();
        boost::replace_all(res, srcroot.string(), "/");
    }
    if (count == 0)
        res += std::string() + EMPTY + '\n';
    else if (res.back() != '\n')
        res += '\n';
    res += '\n';
    if (hasmore) res += "-> Truncated due to too much output\n";
    auto end = high_resolution_clock::now();

    if (watchdog_bitten) {
        res += WDT_BITE_STR;
    } else {
        std::stringstream stream;
        float millis = static_cast<float>(duration_cast<milliseconds>(end - start).count());
        stream << std::fixed << std::setprecision(3) << millis * 0.001;
        res += "-> It took " + stream.str() + " seconds\n";
    }
    pclose(fp);
    ID_LOG_I(unique_id, "%s: ---", __func__);
#undef ID_LOG_I
}

static void commonCleanup(const Bot &bot, const Message::Ptr &message,
                          std::string &res, const std::string &filename = "") {
    bot_sendReplyMessage(bot, message, res);
    if (!filename.empty()) std::remove(filename.c_str());
}

// Verify, Parse, Write
static bool commonVPW(const CompileHandleData &data, std::string &extraargs) {
    bool ret = false;
    if (hasExtArgs(data.message)) {
        parseExtArgs(data.message, extraargs);
        if (extraargs == "--path") {
            bot_sendReplyMessage(data.bot, data.message,
                                 "Selected compiler: " + data.cmdPrefix);
            return ret;  // Bail out
        }
    }
    ret = verifyMessage(data.bot, data.message) &&
          writeMessageToFile(data.bot, data.message, data.outfile);
    return ret;
}

template <>
void CompileRunHandler<CCppCompileHandleData>(const CCppCompileHandleData &data) {
    std::string res, extraargs;
    std::stringstream cmd;
#ifdef __WIN32
    const char aoutname[] = "./a.exe";
#else
    const char aoutname[] = "./a.out";
#endif

    if (commonVPW(data, extraargs)) {
        cmd << data.cmdPrefix << SPACE << data.outfile;
        addExtArgs(cmd, extraargs, res);

        res += "Compile time:\n";
        runCommand(data.bot, data.message, cmd.str(), res);
        res += "\n";

        if (fileExists(aoutname)) {
            res += "Run time:\n";
            runCommand(data.bot, data.message, aoutname, res);
            std::filesystem::remove(aoutname);
        }
        commonCleanup(data.bot, data.message, res, data.outfile);
    }
}

template <>
void CompileRunHandler(const CompileHandleData &data) {
    std::string res, extargs;
    std::stringstream cmd;

    if (commonVPW(data, extargs)) {
        cmd << data.cmdPrefix << SPACE << data.outfile;
        addExtArgs(cmd, extargs, res);

        runCommand(data.bot, data.message, cmd.str(), res);
        commonCleanup(data.bot, data.message, res, data.outfile);
    }
}

template <>
void CompileRunHandler<BashHandleData>(const BashHandleData &data) {
    std::string res;
    if (hasExtArgs(data.message)) {
        std::string cmd;
        parseExtArgs(data.message, cmd);
        runCommand(data.bot, data.message, cmd, res, !data.allowhang);
    } else {
        res = "Send a bash command to run";
    }
    commonCleanup(data.bot, data.message, res);
}

static std::optional<std::string> findCommandExe(std::string command) {
    static std::string path;
    static std::once_flag once;
    static bool valid;

    std::call_once(once, [] { 
        auto it = ConfigManager::getVariable("PATH");
        valid = it.has_value();
        if (valid) {
            path = it.value();
        }
    });
    if (valid) {
        auto paths = StringTools::split(path, path_env_delimiter);
#ifdef __WIN32
        command.append(".exe");
#endif
        for (const auto &path : paths) {
            if (!isEmptyOrBlank(path)) {
                std::filesystem::path p(path);
                p /= command;
                if (canExecute(p.string())) {
                    return {p.string()};
                }
            }
        }
    }
    return {};
}

#define COMPILER(lang, ...)                     \
    array_helpers::make_elem<ProgrammingLangs,  \
    std::vector<std::string>>(lang, {__VA_ARGS__})

bool findCompiler(ProgrammingLangs lang, std::string &path) {
    static const auto compilers = array_helpers::make<ProgrammingLangs::MAX,
        ProgrammingLangs, const std::vector<std::string>>(
        COMPILER(ProgrammingLangs::C, "clang", "gcc", "cc"),
        COMPILER(ProgrammingLangs::CXX, "clang++", "g++", "c++"),
        COMPILER(ProgrammingLangs::GO, "go"),
        COMPILER(ProgrammingLangs::PYTHON, "python", "python3")
    );
    for (const auto &options : array_helpers::find(compilers, lang)->second) {
        auto ret = findCommandExe(options);
        if (ret) {
            path = ret.value();
            return true;
        }
    }
    return false;
}
