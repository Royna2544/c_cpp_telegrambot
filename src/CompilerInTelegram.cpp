#include <Authorization.h>
#include <BotReplyMessage.h>
#include <CompilerInTelegram.h>
#include <ConfigManager.h>
#include <EnumArrayHelpers.h>
#include <ExtArgs.h>
#include <Logging.h>
#include <StringToolsExt.h>
#include <libos/libfs.h>
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

bool HandleData::verifyMessage() {
    if (message->replyToMessage && !message->replyToMessage->text.empty()) {
        return true;
    }
    onFailed(ErrorType::MESSAGE_VERIFICATION_FAILED);
    return false;
}

bool HandleData::writeMessageToFile(const std::string &filename) {
    std::ofstream file(filename);
    if (file.fail()) {
        onFailed(ErrorType::FILE_WRITE_FAILED);
        return false;
    }
    file << message->replyToMessage->text;
    file.close();
    return true;
}

void HandleData::appendExtArgs(std::stringstream &cmd,
                               std::string &extraargs_in,
                               std::string &result_out) {
    if (!extraargs_in.empty()) {
        auto idx = extraargs_in.find_first_of("\n\r\t");
        if (idx != std::string::npos) extraargs_in.erase(idx);
        cmd << SPACE << extraargs_in;
        result_out += "cmd: \"";
        result_out += cmd.str();
        result_out += "\"\n";
    }
}

void HandleData::runCommand(std::string cmd, std::string &res, bool use_wdt) {
    bool hasmore = false, watchdog_bitten = false;
    int count = 0, unique_id = 0;
    char buf[BASH_READ_BUF] = {};
    std::error_code ec;

#ifdef LOCALE_EN_US
    static std::once_flag once;
    std::call_once(once, [] { setlocale_enus_once(); });
#endif

    unique_id = genRandomNumber(100);
    boost::replace_all(cmd, std::string(1, '"'), "\\\"");

#define ID_LOG_I(id, fmt, ...) LOG_I("[ID %d] " fmt, id, ##__VA_ARGS__)
    ID_LOG_I(unique_id, "%s: +++", __func__);
    ID_LOG_I(unique_id, "Command: %s", cmd.c_str());
    auto start = high_resolution_clock::now();
    auto fp = popen_watchdog(cmd.c_str(), use_wdt ? &watchdog_bitten : nullptr);

    if (!fp) {
        onFailed(ErrorType::POPEN_WDT_FAILED);
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
        float millis = static_cast<float>(
            duration_cast<milliseconds>(end - start).count());
        stream << std::fixed << std::setprecision(3) << millis * 0.001;
        res += "-> It took " + stream.str() + " seconds\n";
    }
    pclose(fp);
    ID_LOG_I(unique_id, "%s: ---", __func__);
#undef ID_LOG_I
}

void HandleData::commonCleanup(const std::string &res,
                               const std::string &filename) {
    onResultReady(res);
    if (!filename.empty()) std::filesystem::remove(filename);
}

void HandleDataImpl::onFailed(const ErrorType e) {
    std::string text;
    switch (e) {
        case HandleData::ErrorType::MESSAGE_VERIFICATION_FAILED:
            text = "Message verification failed";
            break;
        case HandleData::ErrorType::FILE_WRITE_FAILED:
            text = "Failed to write to file";
            break;
        case HandleData::ErrorType::POPEN_WDT_FAILED:
            text = "Failed to run command";
            break;
    };
    bot_sendReplyMessage(_bot, message, text);
}

void HandleDataImpl::onResultReady(const std::string &text) {
    bot_sendReplyMessage(_bot, message, text);
}

void CompileHandleData::onCompilerPathCommand(const std::string &text) {
    bot_sendReplyMessage(_bot, message, text);
}

// Verify, Parse, Write
bool CompileHandleData::commonVPW(std::string &extraargs) {
    bool ret = false;
    if (hasExtArgs(message)) {
        parseExtArgs(message, extraargs);
        if (extraargs == "--path") {
            onCompilerPathCommand("Selected compiler: " + cmdPrefix);
            return ret;  // Bail out
        }
    }
    ret = verifyMessage() && writeMessageToFile(outfile);
    return ret;
}

void CCppCompileHandleData::run() {
    std::string res, extraargs;
    std::stringstream cmd;
#ifdef __WIN32
    const char aoutname[] = "./a.exe";
#else
    const char aoutname[] = "./a.out";
#endif

    if (commonVPW(extraargs)) {
        cmd << cmdPrefix << SPACE << outfile;
        appendExtArgs(cmd, extraargs, res);

        res += "Compile time:\n";
        runCommand(cmd.str(), res);
        res += "\n";

        if (fileExists(aoutname)) {
            res += "Run time:\n";
            runCommand(aoutname, res);
            std::filesystem::remove(aoutname);
        }
        commonCleanup(res, outfile);
    }
}

void CompileHandleData::run() {
    std::string res, extargs;
    std::stringstream cmd;

    if (commonVPW(extargs)) {
        cmd << cmdPrefix << SPACE << outfile;
        appendExtArgs(cmd, extargs, res);

        runCommand(cmd.str(), res);
        commonCleanup(res, outfile);
    }
}

void BashHandleData::run() {
    std::string res;
    if (hasExtArgs(message)) {
        std::string cmd;
        parseExtArgs(message, cmd);
        runCommand(cmd, res, !allowhang);
    } else {
        res = "Send a bash command to run";
    }
    commonCleanup(res);
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

#define COMPILER(lang, ...)                                               \
    array_helpers::make_elem<ProgrammingLangs, std::vector<std::string>>( \
        lang, {__VA_ARGS__})

bool findCompiler(ProgrammingLangs lang, std::string &path) {
    static const auto compilers =
        array_helpers::make<ProgrammingLangs::MAX, ProgrammingLangs,
                            const std::vector<std::string>>(
            COMPILER(ProgrammingLangs::C, "clang", "gcc", "cc"),
            COMPILER(ProgrammingLangs::CXX, "clang++", "g++", "c++"),
            COMPILER(ProgrammingLangs::GO, "go"),
            COMPILER(ProgrammingLangs::PYTHON, "python", "python3"));
    for (const auto &options : array_helpers::find(compilers, lang)->second) {
        auto ret = findCommandExe(options);
        if (ret) {
            path = ret.value();
            return true;
        }
    }
    return false;
}
