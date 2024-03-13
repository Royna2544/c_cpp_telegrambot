#include <CompilerInTelegram.h>
#include <ConfigManager.h>
#include <EnumArrayHelpers.h>
#include <Logging.h>
#include <StringToolsExt.h>
#include <libos/libfs.hpp>
#include <random/RandomNumberGenerator.h>

#include <boost/algorithm/string/replace.hpp>
#include <chrono>
#include <mutex>
#include <thread>

#include "popen_wdt/popen_wdt.h"

using std::chrono_literals::operator""ms;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

bool CompilerInTg::verifyAndWriteMessage(const Message::Ptr &message,
                                         const std::string &filename) {
    if (message->replyToMessage && !message->replyToMessage->text.empty()) {
        std::ofstream file(filename);
        if (file.fail()) {
            onFailed(message, ErrorType::FILE_WRITE_FAILED);
            return false;
        }
        file << message->replyToMessage->text;
        file.close();
        return true;
    }
    onFailed(message, ErrorType::MESSAGE_VERIFICATION_FAILED);
    return false;
}

void CompilerInTg::appendExtArgs(std::stringstream &cmd,
                                 std::string extraargs_in,
                                 std::stringstream &result_out) {
    if (!extraargs_in.empty()) {
        TrimStr(extraargs_in);
        cmd << SPACE << extraargs_in;
        std::stringstream result;
        result_out << "cmd: ";
        result_out << std::quoted(cmd.str());
        result_out << std::endl;
    }
}

void CompilerInTg::runCommand(const Message::Ptr &message, std::string cmd,
                              std::stringstream &res, bool use_wdt) {
    bool hasmore = false, watchdog_bitten = false;
    int count = 0, unique_id = 0;
    char buf[BASH_READ_BUF] = {};
    size_t buf_len = 0;
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
        onFailed(message, ErrorType::POPEN_WDT_FAILED);
        return;
    }
    while (fgets(buf, sizeof(buf), fp)) {
        if (buf_len < BASH_MAX_BUF) {
            res << buf;
            buf_len += strlen(buf);
        } else {
            hasmore = true;
        }
        count++;
        std::this_thread::sleep_for(50ms);
    }
    if (count == 0) res << EMPTY << std::endl;
    res << std::endl;
    if (hasmore) res << "-> Truncated due to too much output\n";
    auto end = high_resolution_clock::now();

    if (watchdog_bitten) {
        res << WDT_BITE_STR;
    } else {
        float millis = static_cast<float>(
            duration_cast<milliseconds>(end - start).count());
        res << "-> It took " << std::fixed << std::setprecision(3)
            << millis * 0.001 << " seconds" << std::endl;
    }
    pclose(fp);
    ID_LOG_I(unique_id, "%s: ---", __func__);
#undef ID_LOG_I
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
