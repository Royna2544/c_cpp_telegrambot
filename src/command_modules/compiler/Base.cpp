#include <ConfigManager.h>
#include <EnumArrayHelpers.h>
#include <StringToolsExt.h>
#include <absl/log/log.h>

#include <DurationPoint.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <libos/libfs.hpp>
#include <mutex>
#include <ostream>
#include <thread>

#include "CompilerInTelegram.h"
#include "popen_wdt/popen_wdt.h"

using std::chrono_literals::operator""ms;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

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
    bool hasmore = false;
    int count = 0;
    std::array<char, BASH_READ_BUF> buf = {};
    size_t buf_len = 0;
    popen_watchdog_data_t *p_wdt_data = nullptr;

    boost::replace_all(cmd, std::string(1, '"'), "\\\"");

    LOG(INFO) << __func__ << ": +++";
    onFailed(message, ErrorType::START_COMPILER);
    LOG(INFO) << "Command: '" << cmd << "'";

    auto dp = DurationPoint();

    if (popen_watchdog_init(&p_wdt_data)) {
        p_wdt_data->command = cmd.c_str();
        p_wdt_data->watchdog_enabled = use_wdt;
        popen_watchdog_start(p_wdt_data);

        if (p_wdt_data->fp == nullptr) {
            onFailed(message, ErrorType::POPEN_WDT_FAILED);
            return;
        }
        while (fgets(buf.data(), buf.size(), p_wdt_data->fp) != nullptr) {
            if (buf_len < BASH_MAX_BUF) {
                res << buf.data();
                buf_len += strlen(buf.data());
            } else {
                hasmore = true;
            }
            count++;
            std::this_thread::sleep_for(50ms);
        }
        if (count == 0) {
            res << EMPTY << std::endl;
        }
        res << std::endl;
        if (hasmore) {
            res << "-> Truncated due to too much output" << std::endl;
        }

        if (popen_watchdog_activated(p_wdt_data)) {
            res << WDT_BITE_STR;
        } else {
            double millis = static_cast<double>(dp.get().count()) * 0.001;
            res << "-> It took " << std::fixed << std::setprecision(3) << millis
                << " seconds" << std::endl;
            if (use_wdt) popen_watchdog_stop(p_wdt_data);
        }
        LOG(INFO) << __func__ << ": ---";
    }
}

static std::optional<std::string> findCommandExe(const std::string &command) {
    static std::vector<std::string> paths;
    static std::once_flag once;

    std::call_once(once, [] {
        auto it = ConfigManager::getVariable(ConfigManager::Configs::PATH);
        if (it.has_value()) {
            paths = StringTools::split(it.value(), FS::path_env_delimiter);
        } else {
            throw std::runtime_error("PATH cannot be empty");
        }
    });
    std::filesystem::path exePath(command);
    FS::appendExeExtension(exePath);
    for (const auto &path : paths) {
        if (!isEmptyOrBlank(path)) {
            std::filesystem::path p(path);
            p /= exePath;
            if (FS::canExecute(p)) {
                return {p.string()};
            }
        }
    }
    return {};
}

array_helpers::ArrayElem<ProgrammingLangs, std::vector<std::string>> COMPILER(
    ProgrammingLangs &&lang, std::initializer_list<std::string> &&v) {
    return array_helpers::make_elem<ProgrammingLangs, std::vector<std::string>>(
        std::move(lang), std::move(v));
}

bool findCompiler(ProgrammingLangs lang, std::filesystem::path &path) {
    static const auto compilers =
        array_helpers::make<static_cast<int>(ProgrammingLangs::MAX),
                            ProgrammingLangs, const std::vector<std::string>>(
            COMPILER(ProgrammingLangs::C, {"clang", "gcc", "cc"}),
            COMPILER(ProgrammingLangs::CXX, {"clang++", "g++", "c++"}),
            COMPILER(ProgrammingLangs::GO, {"go"}),
            COMPILER(ProgrammingLangs::PYTHON, {"python", "python3"}));
    for (const auto &options : array_helpers::find(compilers, lang)->second) {
        auto ret = findCommandExe(options);
        if (ret) {
            path = ret.value();
            return true;
        }
    }
    return false;
}
