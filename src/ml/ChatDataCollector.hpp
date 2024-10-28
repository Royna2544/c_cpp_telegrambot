#include <Types.h>

#include <api/TgBotApiImpl.hpp>
#include <ctime>
#include <fstream>
#include <libos/OnTerminateRegistrar.hpp>
#include "api/TgBotApi.hpp"
#include "trivial_helpers/fruit_inject.hpp"

// Collects user data to a CSV
class ChatDataCollector {
   public:
    struct Data {
        ChatId chatId{};
        UserId userId{};
        std::time_t timestamp{};
        enum class MsgType {
            TEXT,
            PHOTO,
            VIDEO,
            DOCUMENT,
            STICKER,
            GIF,
            ETC
        } msgType{};

        Data() = default;
        explicit Data(const Message::Ptr& message) {
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
            chatId = message->chat->id;
            userId = message->from->id;
            timestamp = message->date;
        }
    };

    void onMessage(const Message::Ptr& message) {
        chatData.emplace_back(message);
    }

    APPLE_EXPLICIT_INJECT(ChatDataCollector(TgBotApi::Ptr api));
    ~ChatDataCollector();

   private:
    std::vector<Data> chatData;
};

inline std::ostream& operator<<(std::ostream& os, ChatDataCollector::Data d) {
    os << d.chatId << "," << d.userId << "," << d.timestamp << ","
       << static_cast<int>(d.msgType) << "\n";
    return os;
}


inline ChatDataCollector::ChatDataCollector(TgBotApi::Ptr api) {
    api->onAnyMessage([this](TgBotApi::CPtr api, const Message::Ptr& message) {
        onMessage(message);
        return TgBotApiImpl::AnyMessageResult::Handled;
    });
}

inline ChatDataCollector::~ChatDataCollector() {
    bool existed = false;
    constexpr const char* kChatDataFile = "chat_data.csv";

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
        chatDataFile.open(kChatDataFile, std::ios::app);
    } else {
        chatDataFile.open(kChatDataFile);
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