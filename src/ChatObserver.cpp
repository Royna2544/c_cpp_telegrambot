#include <ChatObserver.h>
#include <utils/libutils.h>

#define CHATLOGF(msg, ...) printf("ChatLog: " msg "\n", ##__VA_ARGS__)

using TgBot::Chat;

std::vector<ChatId> gObservedChatIds;

void processObservers(const Message::Ptr& msg) {
    auto chat = msg->chat;
    auto from = msg->from;
    if (from && chat) {
        auto it = std::find(gObservedChatIds.begin(), gObservedChatIds.end(),
                            chat->id);
        if (it != gObservedChatIds.end()) {
            if (chat->type != Chat::Type::Supergroup) {
                LOG_W("Removing chat '%s' from observer: Not a supergroup", chat->title.c_str());
                gObservedChatIds.erase(it);
		return;
            }
            std::string msgtext;
            if (msg->sticker)
                msgtext = "Sticker";
            else if (msg->animation)
                msgtext = "GIF";
            else
                msgtext = msg->text;
            CHATLOGF("Chat '%s': %s: %s", chat->title.c_str(),
                     from->username.c_str(), msgtext.c_str());
        }
    }
}
