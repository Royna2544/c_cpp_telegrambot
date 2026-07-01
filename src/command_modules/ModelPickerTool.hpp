#pragma once

#include <absl/log/log.h>
#include <absl/strings/numbers.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>

#include <api/TgBotApi.hpp>
#include <api/typedefs.h>

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "support/KeyBoardBuilder.hpp"

// Inline-keyboard model picker for /ask's `models` subcommand, plus the
// per-chat selected-model store (hoisted here from ask.cpp since both files
// need it now). Registers its own callback-query listener under "ask" -
// coexists with AskConfirmTool.hpp's listener, also under "ask", since
// OnCallbackQueryImpl::listeners is a multimap and each listener self-filters
// on its own callback_data prefix.
namespace llm::model_picker {

constexpr std::string_view kCallbackPrefix = "ask_model:";
constexpr std::size_t kPageSize = 8;

// --- Per-chat selected model (hoisted from ask.cpp) ---
inline std::pair<std::mutex&, std::unordered_map<ChatId, std::string>&>
selectedModelStore() {
    static std::mutex mtx;
    static std::unordered_map<ChatId, std::string> map;
    return {mtx, map};
}
inline std::string selectedModel(ChatId chatId) {
    auto [mtx, map] = selectedModelStore();
    const std::lock_guard lock(mtx);
    auto it = map.find(chatId);
    return it == map.end() ? std::string{} : it->second;
}
inline void setSelectedModel(ChatId chatId, std::string model) {
    auto [mtx, map] = selectedModelStore();
    const std::lock_guard lock(mtx);
    map[chatId] = std::move(model);
}

// --- Per-chat picker state (list snapshot from listModels() + current page) ---
struct ModelPickerState {
    std::vector<std::string> modelIds;
    std::size_t page{0};
};
inline std::pair<std::mutex&, std::unordered_map<ChatId, ModelPickerState>&>
modelPickerStore() {
    static std::mutex mtx;
    static std::unordered_map<ChatId, ModelPickerState> map;
    return {mtx, map};
}

inline TgBot::InlineKeyboardMarkup::Ptr buildModelPage(
    const std::vector<std::string>& ids, std::size_t page) {
    KeyboardBuilder builder(2);
    const std::size_t start = page * kPageSize;
    const std::size_t end = std::min(start + kPageSize, ids.size());
    for (std::size_t i = start; i < end; ++i) {
        std::string label = ids[i];
        if (label.size() > 30) {
            label = label.substr(0, 27) + "...";
        }
        builder.addKeyboard(
            KeyboardBuilder::Button{label, fmt::format("{}s:{}", kCallbackPrefix, i)});
    }
    std::vector<KeyboardBuilder::Button> navRow;
    if (page > 0) {
        navRow.push_back(
            {"<< Prev", fmt::format("{}p:{}", kCallbackPrefix, page - 1)});
    }
    if (end < ids.size()) {
        navRow.push_back(
            {"Next >>", fmt::format("{}p:{}", kCallbackPrefix, page + 1)});
    }
    if (!navRow.empty()) {
        builder.addKeyboard(navRow);
    }
    return builder.get();
}

// A CallbackQuery's `message` field may be an inaccessible (too-old) message
// - same variant-unwrapping pattern used in kernelbuild.cpp.
inline TgBot::Message::Ptr resolveQueryMessage(
    const TgBot::CallbackQuery::Ptr& query) {
    if (query->message) {
        if (auto* msg = std::get_if<TgBot::Message::Ptr>(&(*query->message))) {
            return *msg;
        }
    }
    return nullptr;
}

inline void ensureListenerRegistered(TgBotApi::Ptr api) {
    static std::once_flag flag;
    std::call_once(flag, [api] {
        api->onCallbackQuery("ask", [api](TgBot::CallbackQuery::Ptr query) {
            std::string_view data = query->data;
            if (!absl::ConsumePrefix(&data, kCallbackPrefix)) {
                return;  // Not one of ours.
            }
            auto msg = resolveQueryMessage(query);
            if (!msg) {
                LOG(WARNING) << "Model picker callback on an inaccessible "
                               "message; ignoring.";
                return;
            }
            const ChatId chatId = msg->chat->id;

            std::vector<std::string> modelIds;
            {
                auto [mtx, map] = modelPickerStore();
                const std::lock_guard lock(mtx);
                auto it = map.find(chatId);
                if (it == map.end()) {
                    api->answerCallbackQuery(
                        query->id, "This model picker has expired.", true);
                    return;
                }
                modelIds = it->second.modelIds;
            }

            if (absl::ConsumePrefix(&data, "s:")) {
                std::size_t index = 0;
                if (!absl::SimpleAtoi(data, &index) ||
                    index >= modelIds.size()) {
                    api->answerCallbackQuery(query->id, "Invalid selection.",
                                            true);
                    return;
                }
                setSelectedModel(chatId, modelIds[index]);
                try {
                    api->editMessage(
                        msg, fmt::format("Selected model: {}", modelIds[index]));
                } catch (const std::exception& ex) {
                    LOG(WARNING) << "Failed to confirm model selection: "
                                << ex.what();
                }
                {
                    auto [mtx, map] = modelPickerStore();
                    const std::lock_guard lock(mtx);
                    map.erase(chatId);
                }
                api->answerCallbackQuery(query->id, "Model selected.");
            } else if (absl::ConsumePrefix(&data, "p:")) {
                std::size_t page = 0;
                if (!absl::SimpleAtoi(data, &page)) {
                    api->answerCallbackQuery(query->id, "Invalid page.", true);
                    return;
                }
                {
                    auto [mtx, map] = modelPickerStore();
                    const std::lock_guard lock(mtx);
                    if (auto it = map.find(chatId); it != map.end()) {
                        it->second.page = page;
                    }
                }
                try {
                    api->editMessage(msg, msg->text.value_or(""),
                                    buildModelPage(modelIds, page));
                } catch (const std::exception& ex) {
                    LOG(WARNING) << "Failed to paginate model picker: "
                                << ex.what();
                }
                api->answerCallbackQuery(query->id);
            }
        });
    });
}

// Called by ask.cpp's `models` subcommand: snapshots `modelIds` for `chatId`
// at page 0, registers the listener (once, process-wide), and returns the
// rendered first-page keyboard to send alongside the reply.
inline TgBot::InlineKeyboardMarkup::Ptr startPicker(
    TgBotApi::Ptr api, ChatId chatId, std::vector<std::string> modelIds) {
    auto keyboard = buildModelPage(modelIds, 0);
    {
        auto [mtx, map] = modelPickerStore();
        const std::lock_guard lock(mtx);
        map[chatId] = ModelPickerState{std::move(modelIds), 0};
    }
    ensureListenerRegistered(api);
    return keyboard;
}

}  // namespace llm::model_picker
