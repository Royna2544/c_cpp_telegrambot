#pragma once

#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <api/typedefs.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <api/AuthContext.hpp>
#include <api/TgBotApi.hpp>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "llm/LLMBackend.hpp"
#include "llm/OutboundDispatch.hpp"
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
    std::condition_variable_any cv;
    std::optional<ConfirmationAnswer> result;
    bool closed{};
    UserId initiatingUserId{};
    const AuthContext* auth{};
};

inline void queueCallbackAnswer(TgBotApi::Ptr api, std::string callbackId,
                                std::string text = {}, bool alert = false) {
    if (!llm::outbound::post(
            api, [api, callbackId = std::move(callbackId),
                  text = std::move(text), alert](std::stop_token stop) {
                if (!stop.stop_requested()) {
                    api->answerCallbackQuery(callbackId, text, alert);
                }
            })) {
        LOG(ERROR) << "Outbound lane rejected an /ask confirmation callback "
                      "answer";
    }
}

// Same first-name(+last-name) display-name convention used in q.cpp.
inline std::string displayName(const TgBot::User::Ptr& user) {
    if (user->lastName) {
        return fmt::format("{} {}", user->firstName, *user->lastName);
    }
    return user->firstName;
}

// Meyer's-singleton accessor, same idiom as ask.cpp's selectedModelStore().
inline std::pair<
    std::mutex&,
    std::unordered_map<std::string, std::shared_ptr<PendingConfirmation>>&>
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
inline void handleCallback(TgBotApi::Ptr api,
                           const TgBot::CallbackQuery::Ptr& query) {
    std::string_view data = query->data;
    if (!absl::ConsumePrefix(&data, kAskConfirmPrefix)) {
        return;
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
        queueCallbackAnswer(api, query->id, "This confirmation has expired.",
                            true);
        return;
    }
    const bool isInitiator =
        query->from && query->from->id == pending->initiatingUserId;
    const bool remainsAuthorized =
        !pending->auth || pending->auth->isAuthorized(
                              query->from, AuthContext::AccessLevel::AdminUser);
    if (!isInitiator || !remainsAuthorized) {
        queueCallbackAnswer(
            api, query->id,
            "Only the initiating admin can answer this confirmation.", true);
        return;
    }
    if (choice != "y" && choice != "n" && choice != "c") {
        queueCallbackAnswer(api, query->id, "Invalid confirmation choice.",
                            true);
        return;
    }
    bool recorded = false;
    {
        const std::lock_guard lock(pending->mtx);
        if (!pending->closed && !pending->result) {
            pending->result =
                ConfirmationAnswer{choice, displayName(query->from)};
            pending->closed = true;
            recorded = true;
        }
    }
    if (recorded) {
        pending->cv.notify_all();
        queueCallbackAnswer(api, query->id, "Recorded.");
    } else {
        queueCallbackAnswer(api, query->id,
                            "This confirmation was already answered or has "
                            "expired.",
                            true);
    }
}

inline void ensureListenerRegistered(TgBotApi::Ptr api) {
    static std::once_flag flag;
    std::call_once(flag, [api] {
        api->onCallbackQuery("ask", [api](TgBot::CallbackQuery::Ptr query) {
            handleCallback(api, query);
        });
    });
}

inline const llm::Tool kAskConfirmTool{
    "ask",
    "Ask the admin a yes/no/cancel confirmation question via a Telegram "
    "inline keyboard, and wait for their response before proceeding. Before "
    "send_message or save_chat_info, include action_tool and action_input "
    "containing the exact tool name and arguments you will use. Approval is "
    "bound to those exact arguments and consumed once; a different action "
    "will be rejected. Returns \"yes\", \"no\", \"cancel\", or a "
    "no_response message if nobody answered in time.",
    nlohmann::json{
        {"type", "object"},
        {"properties",
         {{"question", {{"type", "string"}}},
          {"action_tool",
           {{"type", "string"}, {"enum", {"send_message", "save_chat_info"}}}},
          {"action_input", {{"type", "object"}}}}},
        {"required", {"question"}},
        {"additionalProperties", false}}};

using ApprovalRecorder =
    std::function<bool(const std::string&, const nlohmann::json&)>;

inline llm::ToolExecutor makeAskConfirmExecutor(
    TgBotApi::Ptr api, ChatId chatId, UserId initiatingUserId,
    const AuthContext* auth, ApprovalRecorder recordApproval = {},
    std::stop_token cancellation = {}) {
    return [api, chatId, initiatingUserId, auth,
            recordApproval = std::move(recordApproval), cancellation](
               const std::string& /*name*/, const nlohmann::json& input,
               bool& isError) -> std::string {
        isError = false;
        std::string question;
        std::optional<std::string> actionTool;
        std::optional<nlohmann::json> actionInput;
        try {
            question = input.at("question").get<std::string>();
            const bool hasActionTool = input.contains("action_tool");
            const bool hasActionInput = input.contains("action_input");
            if (hasActionTool != hasActionInput) {
                throw std::invalid_argument(
                    "action_tool and action_input must be supplied together");
            }
            if (hasActionTool) {
                actionTool = input.at("action_tool").get<std::string>();
                actionInput = input.at("action_input");
                if (!actionInput->is_object()) {
                    throw std::invalid_argument(
                        "action_input must be an object");
                }
                if (!recordApproval) {
                    throw std::invalid_argument(
                        "this confirmation cannot bind an action");
                }
            }
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
        auto keyboard = builder.get();

        auto pending = std::make_shared<PendingConfirmation>();
        pending->initiatingUserId = initiatingUserId;
        pending->auth = auth;
        ensureListenerRegistered(api);
        {
            auto [mtx, map] = confirmationRegistry();
            const std::lock_guard lock(mtx);
            map.emplace(token, pending);
        }

        TgBot::Message::Ptr sent;
        try {
            const auto outboundResult = llm::outbound::invoke(
                api,
                [api, chatId, question, keyboard] {
                    return api->sendMessage(chatId, question, keyboard);
                },
                cancellation);
            if (!outboundResult || !*outboundResult) {
                throw std::runtime_error(
                    "outbound queue unavailable or timed out");
            }
            sent = *outboundResult;
        } catch (const std::exception& ex) {
            {
                const std::lock_guard pendingLock(pending->mtx);
                pending->closed = true;
            }
            auto [mtx, map] = confirmationRegistry();
            const std::lock_guard lock(mtx);
            map.erase(token);
            isError = true;
            return fmt::format("Failed to send confirmation prompt: {}",
                               ex.what());
        }

        std::optional<ConfirmationAnswer> resultSnapshot;
        {
            std::unique_lock lock(pending->mtx);
            pending->cv.wait_for(lock, cancellation, kAskConfirmTimeout,
                                 [&] { return pending->result.has_value(); });
            // Close the token while holding the same lock used by callback
            // delivery. A callback that already looked it up cannot race a
            // timeout/cancellation and record a late approval.
            pending->closed = true;
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
            : cancellation.stop_requested()
                ? "Confirmation cancelled."
                : "No response within the time limit.";
        const auto annotated = fmt::format("{}\n\n{}", question, annotation);
        if (!llm::outbound::post(api, [api, sent,
                                       annotated](std::stop_token stop) {
                if (stop.stop_requested()) {
                    return;
                }
                try {
                    api->editMessage(sent, annotated);
                } catch (const std::exception& ex) {
                    LOG(WARNING) << "Failed to annotate confirmation message: "
                                 << ex.what();
                }
            })) {
            LOG(ERROR) << "Outbound lane rejected confirmation annotation";
        }

        if (!resultSnapshot) {
            if (cancellation.stop_requested()) {
                isError = true;
                return "confirmation cancelled because the /ask work was "
                       "cancelled; do not execute the action";
            }
            return "no_response: the human did not press a button within 90 "
                   "seconds; treat this as unresolved, not as a decision - "
                   "ask again or proceed cautiously";
        }
        if (resultSnapshot->choice == "y") {
            if (actionTool && !recordApproval(*actionTool, *actionInput)) {
                isError = true;
                return "yes, but approval could not be bound to that action; "
                       "do not execute it";
            }
            return "yes";
        }
        if (resultSnapshot->choice == "n") {
            return "no";
        }
        return "cancel";
    };
}

}  // namespace llm::ask_confirm
