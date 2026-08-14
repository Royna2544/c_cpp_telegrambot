#pragma once

#include <api/TgBotApi.hpp>
#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string_view>
#include <type_traits>
#include <utility>

namespace llm::outbound {

inline constexpr std::string_view kOwner = "ask";
inline constexpr std::chrono::seconds kSynchronousWait{35};

inline bool post(TgBotApi::Ptr api, TgBotApi::CancellableWork work) {
    return api && api->submitCommandWork(kOwner, TgBotApi::WorkClass::Outbound,
                                         std::move(work))
                      .has_value();
}

// Executes a Telegram API operation on the Outbound lane while allowing the
// LLM/tool lane to wait for its result. The bounded wait covers queue delay and
// the Outbound lane's 30-second deadline without ever occupying a fast command
// worker.
template <typename Fn>
auto invoke(TgBotApi::Ptr api, Fn&& fn, std::stop_token callerStop = {})
    -> std::optional<std::invoke_result_t<std::decay_t<Fn>>> {
    using Callable = std::decay_t<Fn>;
    using Result = std::invoke_result_t<Callable>;
    static_assert(!std::is_void_v<Result>);

    if (!api || callerStop.stop_requested()) {
        return std::nullopt;
    }

    auto result = std::make_shared<std::promise<Result>>();
    auto future = result->get_future();
    auto workId = api->submitCommandWork(
        kOwner, TgBotApi::WorkClass::Outbound,
        [result, operation = Callable(std::forward<Fn>(fn))](
            std::stop_token stop) mutable {
            if (stop.stop_requested()) {
                result->set_exception(std::make_exception_ptr(
                    std::runtime_error("outbound work was cancelled")));
                return;
            }
            try {
                result->set_value(operation());
            } catch (...) {
                result->set_exception(std::current_exception());
            }
        });
    if (!workId) {
        return std::nullopt;
    }
    const auto deadline = std::chrono::steady_clock::now() + kSynchronousWait;
    while (future.wait_for(std::chrono::milliseconds(50)) !=
           std::future_status::ready) {
        if (callerStop.stop_requested() ||
            std::chrono::steady_clock::now() >= deadline) {
            (void)api->cancelCommandWork(kOwner, *workId);
            return std::nullopt;
        }
    }
    return future.get();
}

}  // namespace llm::outbound
