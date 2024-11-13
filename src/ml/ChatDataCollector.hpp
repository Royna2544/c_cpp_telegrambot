#include <Types.h>

#include <api/TgBotApi.hpp>
#include <ctime>
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

        Data() = default;
        explicit Data(const Message::Ptr& message);
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
