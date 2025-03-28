#pragma once

#include <absl/strings/string_view.h>
#include <json/json.h>

#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <unordered_map>

// Uses VirusTotal API to check virus files.
class FileCheck {
    std::string _token;
    TgBotApi::Ptr _api;

    TgBot::InlineKeyboardMarkup viewFullyKeyboard;
    TgBot::InlineKeyboardMarkup viewShortKeyboard;

    struct ResultHolder {
        std::string result;
        std::string verboseResult;
        Message::Ptr message;

        TgBot::InlineKeyboardMarkup::Ptr summary;
        TgBot::InlineKeyboardMarkup::Ptr all;

        bool verbose_state = false;
    };

    std::unordered_map<int, ResultHolder> _resultHolder;
    int counter = 0;

    static constexpr absl::string_view kQueryDataPrefix = "FileCheck_";

   public:
    explicit FileCheck(TgBotApi::Ptr api, std::string virusTotalToken);
    TgBotApi::AnyMessageResult onAnyMessage(TgBotApi::CPtr api,
                                            const MessageExt::Ptr &message);
    void onCallbackQueryFunction(const TgBot::CallbackQuery::Ptr &query);
};