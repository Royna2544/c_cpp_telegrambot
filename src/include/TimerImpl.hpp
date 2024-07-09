#pragma once

#include <chrono>

#include "ManagedThreads.hpp"

using std::chrono_literals::operator""s;
using std::chrono_literals::operator""h;

// This class represents a timer thread that can start, stop, and parse time
// values.
struct TimerThread : ManagedThreadRunnable {
    static constexpr std::chrono::seconds kMinTime = 5s;
    static constexpr std::chrono::hours kMaxTime = 30h;
    static constexpr std::chrono::seconds kUpdateDuration = 10s;

    TimerThread() = default;
    ~TimerThread() override = default;

    enum class Result {
        TIMER_STATE_INVALID,  // This class state is invalid for this operation.
        TIMER_NOT_SET,        // Nothing was parsed before.
        TIME_CANNOT_PARSE,    // I cannot parse this argument (Only for #parse)
        TIME_TOO_SHORT,       // Time provided is too short (Only for #parse)
        TIME_TOO_LONG,        // Time provided is too long (Only for #parse)
        SUCCESS               // All good.
    };

    /**
     * @brief Starts the timer thread.
     *
     * This function starts the timer thread and sets the timer time to the
     * default value.
     *
     * @return TimerThread::Result The result of the start operation.
     *         - TIMER_STATE_INVALID: This class state is invalid for this
     * operation.
     *         - SUCCESS: All good.
     *         - TIMER_NOT_SET: Nothing was parsed before and not possible to
     * start timer
     */
    Result start();

    /**
     * @brief Stops the timer thread.
     *
     * This function stops the timer thread and resets the timer time to the
     * default value.
     *
     * @return TimerThread::Result The result of the stop operation.
     *         - TIMER_STATE_INVALID: This class state is invalid for this
     * operation.
     *         - SUCCESS: All good.
     */
    Result stop();

    /**
     * @brief Parses a time string into a duration.
     *
     * This function parses a time string into a duration and sets the timer
     * time to the parsed value.
     *
     * @param[in] timeString The time string to parse.
     * @return TimerThread::Result The result of the parse operation.
     *         - TIME_CANNOT_PARSE: I cannot parse this argument.
     *         - TIME_TOO_SHORT: Time provided is too short.
     *         - TIME_TOO_LONG: Time provided is too long.
     *         - SUCCESS: All good.
     */
    Result parse(const std::string& timeString);

    /**
     * @brief Describes the parsed time in a human-readable format.
     *
     * This function retrieves the parsed time from the timer thread and
     * returns it in a human-readable format. If no time has been parsed,
     * the function returns an empty optional.
     *
     * @return std::optional<std::string> The parsed time in a human-readable
     * format, or an empty optional if no time has been parsed.
     */
    [[nodiscard]] std::optional<std::string> describeParsedTime() const;

   protected:
    /**
     * @brief This function is called when the timer thread starts.
     *
     * This function is a virtual function that is called when the timer thread
     * starts. It is intended to be overridden by derived classes to perform any
     * necessary actions when the timer starts.
     *
     * @note This function is called from within the timer thread context.
     *
     * @return void
     */
    virtual void onTimerStart() {};

    /**
     * @brief Callback function to be overridden
     * by derived classes.
     *
     * This function is called when the timer
     * thread reaches a timer point. The function
     * is expected to handle the timer event and
     * update the timer state accordingly.
     *
     * @param[in] timeLeft The remaining time
     * until the next timer point.
     */
    virtual void onTimerPoint(const std::chrono::seconds& timeLeft) {}

    /**
     * @brief Callback function to be overridden
     * by derived classes.
     *
     * This function is called when the timer
     * thread reaches the end of the timer time.
     * The function is expected to handle the timer
     * end event and update the timer state accordingly.
     */
    virtual void onTimerEnd() {}

   private:
    std::optional<std::chrono::seconds> parsedTime;
    std::chrono::seconds timerLeftDuration;
    void runFunction() override;
    bool isactive = false;
};
