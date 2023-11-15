#include <ChatObserver.h>
#include <Logging.h>

#include <iostream>

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
            else if (!msg->photo.empty())
                msgtext = "Photo";
            else if (msg->document)
                msgtext = "File";
            else if (msg->video)
                msgtext = "Video";
            else if (msg->dice)
                msgtext = "(Dice) " + msg->dice->emoji;
            else
                msgtext = msg->text;
            std::string userfullname = from->firstName;
            if (!from->lastName.empty())
                userfullname += ' ' + from->lastName;
            std::cout << "Chat '" << chat->title << "': "
                      << userfullname << " (@" << from->username
                      << "): " << msgtext << std::endl;
        }
    }
}
