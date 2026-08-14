#include <absl/log/log.h>
#include <fmt/core.h>

#include <algorithm>
#include <cstdio>
#include <ios>
#include <libfs.hpp>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

#include "CompilerInTelegram.hpp"
#include "popen_wdt.h"

CompilerInTg::CompilerInTg(std::unique_ptr<Interface> callback,
                           const StringResLoader::PerLocaleMap* loader)
    : _callback(std::move(callback)), _locale(loader) {}

void CompilerInTg::runCommand(std::string cmd, std::stringstream& res,
                              bool use_wdt, std::stop_token stop) {
    bool hasmore = false;
    std::array<char, BASH_READ_BUF> buf = {};
    size_t buf_len = res.str().size();
    popen_watchdog_data_t* p_wdt_data = nullptr;

    LOG(INFO) << __func__ << ": +++";
    _callback->onExecutionStarted(cmd);
    LOG(INFO) << fmt::format("Command is: '{}'", cmd);

    if (!popen_watchdog_init(&p_wdt_data)) {
        LOG(ERROR) << "popen_watchdog_init failed";
        _callback->onErrorStatus(tinystatus::TinyStatus(
            tinystatus::Status::kInternalError, "popen_watchdog_init failed"));
        return;
    }
    p_wdt_data->command = cmd.c_str();
    p_wdt_data->watchdog_enabled = use_wdt;
    DLOG(INFO) << "use_wdt: " << std::boolalpha << p_wdt_data->watchdog_enabled;

    if (!popen_watchdog_start(&p_wdt_data)) {
        LOG(ERROR) << "popen_watchdog_start failed";
        popen_watchdog_destroy(&p_wdt_data);
        _callback->onErrorStatus(tinystatus::TinyStatus(
            tinystatus::Status::kInternalError, "popen_watchdog_start failed"));
        return;
    }
    {
        // The callback's destructor synchronizes with an in-flight cancel, so
        // p_wdt_data cannot be destroyed while cancellation still references
        // it.
        std::stop_callback cancelCallback(
            stop, [&p_wdt_data] { (void)popen_watchdog_cancel(&p_wdt_data); });
        popen_watchdog_ssize_t bytes_read = 0;
        while ((bytes_read = popen_watchdog_read(&p_wdt_data, buf.data(),
                                                 buf.size())) > 0) {
            const auto available =
                buf_len < BASH_MAX_BUF ? BASH_MAX_BUF - buf_len : 0;
            const auto chunk_size = static_cast<size_t>(bytes_read);
            const auto copy_size = std::min(available, chunk_size);
            if (copy_size != 0) {
                res.write(buf.data(), static_cast<std::streamsize>(copy_size));
                buf_len += copy_size;
            }
            if (copy_size != chunk_size) {
                hasmore = true;
            }
        }
        const bool activated = popen_watchdog_activated(&p_wdt_data);
        if (use_wdt && activated) {
            _callback->onWdtTimeout();
        }
    }
    auto ret = popen_watchdog_destroy(&p_wdt_data);
    _callback->onExecutionFinished(cmd, ret);
    if (hasmore) {
        std::string output = res.str();
        const std::string marker =
            fmt::format("\n{}", _locale->get(Strings::IBASH_OUTPUT_TRUNCATED));
        if (marker.size() < BASH_MAX_BUF) {
            output.resize(
                std::min(output.size(), BASH_MAX_BUF - marker.size()));
            output += marker;
        } else {
            output.resize(BASH_MAX_BUF);
        }
        res.str(std::move(output));
        res.clear();
    }
    LOG(INFO) << __func__ << ": ---";
}
