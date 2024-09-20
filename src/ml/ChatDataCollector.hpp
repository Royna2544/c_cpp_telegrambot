#include <ctime>

#include "CStringLifetime.h"
#include "TgBotWrapper.hpp"
#include "Types.h"
#include "database/bot/TgBotDatabaseImpl.hpp"
#include "initcalls/Initcall.hpp"

// Collects user data to a CSV
class ChatDataCollector : public InitCall {
    struct Data {
        ChatId chatId;
        UserId userId;
        std::time_t timestamp;
        enum class MsgType {
            TEXT,
            PHOTO,
            VIDEO,
            DOCUMENT,
            STICKER,
            GIF,
            ETC
        } msgType;
    };

    void doInitCall() override {
        TgBotWrapper::getInstance()->onAnyMessage(
            [this](auto api, auto message) {
                Data::MsgType msgType{};
                if (!message->text.empty()) {
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
                Data data{message->chat->id, message->from->id, message->date,
                          msgType};
                chatData.emplace_back(data);
                return TgBotWrapper::AnyMessageResult::Handled;
            });
        OnTerminateRegistrar::getInstance()->registerCallback(
            [this]() {
                bool existed = false;
                constexpr const char* kChatDataFile = "chat_data.csv";

                if (chatData.empty()) {
                    LOG(INFO)
                        << "No chat data collected, skipping chat data writing";
                    return;
                }
                // Write chat data to chat_data.csv
                if (std::filesystem::exists(kChatDataFile)) {
                    // Then skip writing header
                    existed = true;
                }
                std::ofstream chatDataFile;
                if (existed) {
                    chatDataFile.open(kChatDataFile, std::ios::app);
                } else {
                    chatDataFile.open(kChatDataFile);
                }
                if (!existed) {
                    chatDataFile << "chat_id,user_id,timestamp,message_type\n";
                }
                for (const auto& data : chatData) {
                    chatDataFile << data.chatId << "," << data.userId << ","
                                 << data.timestamp << ","
                                 << static_cast<int>(data.msgType) << "\n";
                }
                LOG(INFO) << "Chat data collected and saved to chat_data.csv: "
                          << chatData.size() << " entries";
            });
    }

    const CStringLifetime getInitCallName() const override {
        return "Setup ChatDataCollector";
    }
    std::vector<Data> chatData;
};