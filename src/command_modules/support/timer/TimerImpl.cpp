#include "TimerImpl.hpp"

#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <trivial_helpers/_std_chrono_templates.h>
#include <trivial_helpers/_tgbot.h>

#include <ManagedThreads.hpp>
#include <api/TgBotApi.hpp>
#include <chrono>
#include <cmath>
#include <sstream>

TimerThread::Result TimerThread::parse(const std::string &timeString) {
    std::chrono::seconds parsedTimeLocal;
    std::string token;
    std::istringstream issTimeStr(timeString);
    std::istringstream issSuffixStr;

    if (isactive) {
        return Result::TIMER_STATE_INVALID;
    }
    parsedTime.reset();
    while (std::getline(issTimeStr, token, ' ')) {
        int num = 0;
        char suffix{};

        if (token.empty()) {
            continue;
        }

        issSuffixStr = std::istringstream(token);
        // Expect <number><h/m/s> format
        if (issSuffixStr >> num && issSuffixStr >> suffix) {
            DLOG(INFO) << "num: " << num << " suffix: " << suffix;
            if (num < 0) {
                LOG(WARNING) << "Negative number: " << num;
                return Result::TIME_CANNOT_PARSE;
            }
            switch (suffix) {
                case 'h':
                    parsedTimeLocal += to_secs(std::chrono::hours(num));
                    break;
                case 'm':
                    parsedTimeLocal += to_secs(std::chrono::minutes(num));
                    break;
                case 's':
                    parsedTimeLocal += to_secs(std::chrono::seconds(num));
                    break;
                default:
                    LOG(WARNING) << "Invalid suffix: " << suffix;
                    return Result::TIME_CANNOT_PARSE;
            }
        } else {
            LOG(WARNING) << "Invalid time format: " << std::quoted(token);
            return Result::TIME_CANNOT_PARSE;
        }
    }
    if (parsedTimeLocal < kMinTime) {
        return Result::TIME_TOO_SHORT;
    } else if (parsedTimeLocal > kMaxTime) {
        return Result::TIME_TOO_LONG;
    }
    parsedTime = parsedTimeLocal;
    return Result::SUCCESS;
}

void TimerThread::runFunction() {
    isactive = true;
    onTimerStart();
    while (timerLeftDuration > 0s && kRun) {
        std::this_thread::sleep_for(1s);
        if (timerLeftDuration % kUpdateDuration == 0s) {
            onTimerPoint(timerLeftDuration);
        }
        timerLeftDuration -= 1s;
    }
    onTimerEnd();
    isactive = false;
}

std::optional<std::string> TimerThread::describeParsedTime() const {
    if (!parsedTime) {
        return std::nullopt;
    }
    return fmt::format("{:%H hours %M minutes %S seconds}", parsedTime.value());
}

TimerThread::Result TimerThread::start() {
    if (isactive) {
        return Result::TIMER_STATE_INVALID;
    }
    if (!parsedTime) {
        return Result::TIMER_NOT_SET;
    }
    timerLeftDuration = parsedTime.value();
    run();
    return Result::SUCCESS;
}

TimerThread::Result TimerThread::stop() {
    if (!isactive) {
        return Result::TIMER_STATE_INVALID;
    }
    ManagedThreadRunnable::stop();
    return Result::SUCCESS;
}
