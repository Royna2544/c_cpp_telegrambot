#include <api/typedefs.h>

#include <api/TgBotApi.hpp>
#include <ctime>
#include <fstream>
#include <map>
#include <mutex>
#include <trivial_helpers/fruit_inject.hpp>

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
        bool isEdited{false};
        bool isForwarded{false};
        UserId replyToUserId{0};
        MessageId messageid;
        MessageId replyToMessageId{0};
        ChatId replyToChatId{0};
        MessageThreadId threadId{0};
        bool is_premium{false};

        Data() = default;
        explicit Data(const Message::Ptr& message);
    };

    void onMessage(const Message::Ptr& message);

    APPLE_EXPLICIT_INJECT(ChatDataCollector(TgBotApi::Ptr api));
    ~ChatDataCollector();

   private:
    std::ofstream chatDataFile;
    std::mutex mutex_;
    std::map<UserId, TgBot::User::Ptr> userDict_;
    std::map<ChatId, TgBot::Chat::Ptr> chatDict_;
};

inline std::ostream& operator<<(std::ostream& os, ChatDataCollector::Data d) {
    os << d.chatId << "," << d.userId << "," << d.timestamp << ","
       << static_cast<int>(d.msgType) << "," << static_cast<int>(d.isEdited)
       << "," << static_cast<int>(d.isForwarded) << "," << d.replyToUserId
       << "," << d.messageid << "," << d.replyToMessageId << ","
       << d.replyToChatId << "," << d.threadId << ","
       << static_cast<int>(d.is_premium) << "\n";
    return os;
}
