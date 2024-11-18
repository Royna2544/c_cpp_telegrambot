#include "ChatDataCollector.hpp"

#include <absl/log/log.h>

#include <filesystem>
#include <fstream>

ChatDataCollector::Data::Data(const Message::Ptr& message) {
    if (!message->text) {
        msgType = Data::MsgType::TEXT;
    } else if (!message->photo.empty()) {
        msgType = Data::MsgType::PHOTO;
    } else if (message->video) {
        msgType = Data::MsgType::VIDEO;
    } else if (message->document) {
        msgType = Data::MsgType::DOCUMENT;
    } else if (message->sticker) {
        msgType = Data::MsgType::STICKER;
    } else if (message->animation) {
        msgType = Data::MsgType::GIF;
    } else {
        msgType = Data::MsgType::ETC;
    }
    chatId = message->chat->id;
    userId = message->from->id;
    timestamp = message->date;
}

ChatDataCollector::ChatDataCollector(TgBotApi::Ptr api) {
    api->onAnyMessage([this](TgBotApi::CPtr api, const Message::Ptr& message) {
        onMessage(message);
        return TgBotApi::AnyMessageResult::Handled;
    });
}

ChatDataCollector::~ChatDataCollector() {
    bool existed = false;
    constexpr std::string_view kChatDataFile = "chat_data.csv";

    if (chatData.empty()) {
        LOG(INFO) << "No chat data collected, skipping chat data writing";
        return;
    }
    // Write chat data to chat_data.csv
    if (std::filesystem::exists(kChatDataFile)) {
        // Then skip writing header
        existed = true;
    }
    std::ofstream chatDataFile;
    if (existed) {
        chatDataFile.open(kChatDataFile.data(), std::ios::app);
    } else {
        chatDataFile.open(kChatDataFile.data());
    }
    if (!existed) {
        chatDataFile << "chat_id,user_id,timestamp,message_type\n";
    }
    for (const auto& data : chatData) {
        chatDataFile << data;
    }
    LOG(INFO) << "Chat data collected and saved to chat_data.csv: "
              << chatData.size() << " entries";
}