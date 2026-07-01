#pragma once

#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <api/TgBotApi.hpp>
#include <api/typedefs.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "llm/LLMBackend.hpp"
#include "support/KeyBoardBuilder.hpp"

// An LLM tool ("ask") that posts a Yes/No/Cancel inline keyboard and blocks
// the tool-calling loop until the admin taps a button (or a timeout
// elapses). Meant as a human-confirmation primitive for other tools to rely
// on before doing something consequential.
namespace llm::ask_confirm {

constexpr std::string_view kAskConfirmPrefix = "ask_confirm:";
constexpr std::chrono::seconds kAskConfirmTimeout{90};

struct ConfirmationAnswer {
    std::string choice;   // "y" | "n" | "c"
    std::string presser;  // display name of whoever pressed the button
};

struct PendingConfirmation {
    std::mutex mtx;
    std::condition_variable cv;
    std::optional<ConfirmationAnswer> result;
};

// Same first-name(+last-name) display-name convention used in q.cpp.
inline std::string displayName(const TgBot::User::Ptr& user) {
    if (user->lastName) {
        return fmt::format("{} {}", user->firstName, *user->lastName);
    }
    return user->firstName;
}

// Meyer's-singleton accessor, same idiom as ask.cpp's selectedModelStore().
inline std::pair<std::mutex&,
                 std::unordered_map<std::string,
                                   std::shared_ptr<PendingConfirmation>>&>
confirmationRegistry() {
    static std::mutex mtx;
    static std::unordered_map<std::string, std::shared_ptr<PendingConfirmation>>
        map;
    return {mtx, map};
}

// Registers the callback-query listener exactly once per process, under the
// key "ask" (this module's own DynModule::name). This matters: `ask.cpp`
// compiles to a dynamically-loaded DLL, and the only way a registered
// callback listener ever gets cleaned up is `OnCallbackQueryImpl::onUnload`
// purging every listener registered under the unloading module's own name.
// Registering under any other key would leave a dangling closure (pointing
// into a possibly-unloaded DLL) permanently registered.
inline void ensureListenerRegistered(TgBotApi::Ptr api) {
    static std::once_flag flag;
    std::call_once(flag, [api] {
        api->onCallbackQuery("ask", [api](TgBot::CallbackQuery::Ptr query) {
            std::string_view data = query->data;
            if (!absl::ConsumePrefix(&data, kAskConfirmPrefix)) {
                return;  // Not one of ours.
            }
            const auto sep = data.find(':');
            if (sep == std::string_view::npos) {
                return;
            }
            const std::string token(data.substr(0, sep));
            const std::string choice(data.substr(sep + 1));

            std::shared_ptr<PendingConfirmation> pending;
            {
                auto [mtx, map] = confirmationRegistry();
                const std::lock_guard lock(mtx);
                if (auto it = map.find(token); it != map.end()) {
                    pending = it->second;
                }
            }
            if (!pending) {
                // Already resolved or timed out (the waiter is the only
                // side that erases registry entries); ack anyway so the
                // button doesn't spin forever in the Telegram client.
                api->answerCallbackQuery(
                    query->id, "This confirmation has expired.", true);
                return;
            }
            {
                const std::lock_guard lock(pending->mtx);
                pending->result =
                    ConfirmationAnswer{choice, displayName(query->from)};
            }
            pending->cv.notify_all();
            api->answerCallbackQuery(query->id, "Recorded.");
        });
    });
}

inline const llm::Tool kAskConfirmTool{
    "ask",
    "Ask the admin a yes/no/cancel confirmation question via a Telegram "
    "inline keyboard, and wait for their response before proceeding. Use "
    "this before taking any action that should require explicit human "
    "confirmation. Returns \"yes\", \"no\", \"cancel\", or a no_response "
    "message if nobody answered in time.",
    nlohmann::json{{"type", "object"},
                  {"properties", {{"question", {{"type", "string"}}}}},
                  {"required", {"question"}}}};

inline llm::ToolExecutor makeAskConfirmExecutor(TgBotApi::Ptr api,
                                                ChatId chatId) {
    return [api, chatId](const std::string& /*name*/,
                        const nlohmann::json& input,
                        bool& isError) -> std::string {
        isError = false;
        std::string question;
        try {
            question = input.at("question").get<std::string>();
        } catch (const std::exception& ex) {
            isError = true;
            return fmt::format("Invalid tool input: {}", ex.what());
        }

        static std::atomic<std::uint64_t> counter{0};
        const std::string token =
            fmt::format("{}_{}", chatId, counter.fetch_add(1));
        const std::string cbYes =
            fmt::format("{}{}:y", kAskConfirmPrefix, token);
        const std::string cbNo =
            fmt::format("{}{}:n", kAskConfirmPrefix, token);
        const std::string cbCancel =
            fmt::format("{}{}:c", kAskConfirmPrefix, token);

        KeyboardBuilder builder(3);
        builder.addKeyboard(
            {{"Yes", cbYes}, {"No", cbNo}, {"Cancel", cbCancel}});

        TgBot::Message::Ptr sent;
        try {
            sent = api->sendMessage(chatId, question, builder.get());
        } catch (const TgBot::TgException& ex) {
            isError = true;
            return fmt::format("Failed to send confirmation prompt: {}",
                              ex.what());
        }

        auto pending = std::make_shared<PendingConfirmation>();
        {
            auto [mtx, map] = confirmationRegistry();
            const std::lock_guard lock(mtx);
            map.emplace(token, pending);
        }
        ensureListenerRegistered(api);

        std::optional<ConfirmationAnswer> resultSnapshot;
        {
            std::unique_lock lock(pending->mtx);
            pending->cv.wait_for(lock, kAskConfirmTimeout,
                                [&] { return pending->result.has_value(); });
            resultSnapshot = pending->result;
        }
        {
            auto [mtx, map] = confirmationRegistry();
            const std::lock_guard lock(mtx);
            map.erase(token);
        }

        // Editing the text (with the default null markup) both records who
        // answered and clears the keyboard in one call.
        const std::string annotation =
            resultSnapshot
                ? fmt::format("({}) pressed {}.", resultSnapshot->presser,
                             resultSnapshot->choice == "y"   ? "Yes"
                             : resultSnapshot->choice == "n" ? "No"
                                                             : "Cancel")
                : "No response within the time limit.";
        try {
            api->editMessage(sent, fmt::format("{}\n\n{}", question, annotation));
        } catch (const std::exception& ex) {
            LOG(WARNING) << "Failed to annotate confirmation message: "
                        << ex.what();
        }

        if (!resultSnapshot) {
            return "no_response: the human did not press a button within 90 "
                  "seconds; treat this as unresolved, not as a decision - "
                  "ask again or proceed cautiously";
        }
        if (resultSnapshot->choice == "y") {
            return "yes";
        }
        if (resultSnapshot->choice == "n") {
            return "no";
        }
        return "cancel";
    };
}

}  // namespace llm::ask_confirm
