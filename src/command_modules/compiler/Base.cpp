#include <ConfigManager.h>
#include <EnumArrayHelpers.h>
#include <absl/log/log.h>
#include <absl/strings/str_replace.h>

#include <DurationPoint.hpp>
#include <StringResManager.hpp>
#include <chrono>
#include <cstdio>
#include <ios>
#include <libos/libfs.hpp>
#include <memory>
#include <ostream>
#include <thread>
#include <utility>

#include "CompilerInTelegram.hpp"
#include "StringToolsExt.hpp"
#include "absl/status/status.h"
#include "popen_wdt.h"

using std::chrono_literals::operator""ms;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

CompilerInTg::CompilerInTg(std::shared_ptr<Interface> interface)
    : _interface(std::move(interface)) {}

void CompilerInTg::runCommand(std::string cmd, std::stringstream &res,
                              bool use_wdt) {
    bool hasmore = false;
    int count = 0;
    std::array<char, BASH_READ_BUF> buf = {};
    size_t buf_len = 0;
    popen_watchdog_data_t *p_wdt_data = nullptr;

    cmd = absl::StrReplaceAll(cmd, {{"\"", "\\\""}});
    LOG(INFO) << __func__ << ": +++";
    _interface->onExecutionStarted(cmd);
    LOG(INFO) << GETSTR_IS(COMMAND) << SingleQuoted(cmd);

    if (!popen_watchdog_init(&p_wdt_data)) {
        LOG(ERROR) << "popen_watchdog_init failed";
        _interface->onErrorStatus(
            absl::InternalError("popen_watchdog_init failed"));
        return;
    }
    p_wdt_data->command = cmd.c_str();
    p_wdt_data->watchdog_enabled = use_wdt;
    DLOG(INFO) << "use_wdt: " << std::boolalpha << p_wdt_data->watchdog_enabled;

    if (!popen_watchdog_start(&p_wdt_data)) {
        LOG(ERROR) << "popen_watchdog_start failed";
        popen_watchdog_destroy(&p_wdt_data);
        _interface->onErrorStatus(
            absl::InternalError("popen_watchdog_start failed"));
        return;
    }
    while (popen_watchdog_read(&p_wdt_data, buf.data(), buf.size() - 1)) {
        if (buf_len < BASH_MAX_BUF) {
            res << buf.data();
            buf_len += strlen(buf.data());
        } else {
            hasmore = true;
        }
        buf.fill(0);
        count++;
        std::this_thread::sleep_for(50ms);
    }
    if (use_wdt) {
        if (popen_watchdog_activated(&p_wdt_data)) {
            _interface->onWdtTimeout();
        }
    }
    popen_watchdog_destroy(&p_wdt_data);
    _interface->onExecutionFinished(cmd);
    LOG(INFO) << __func__ << ": ---";
}
