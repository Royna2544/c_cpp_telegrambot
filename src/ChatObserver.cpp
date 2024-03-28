#include <ChatObserver.h>
#include <Logging.h>

#include <algorithm>
#include <iostream>
#include <mutex>

#include "internal/_tgbot.h"

using TgBot::Message;

void ChatObserver::printChatMsg(const Message::Ptr& msg, const User::Ptr& from) {
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

    std::cout << "[ChatObserveLog][" << ChatPtr_toString(msg->chat) << "] "
              << UserPtr_toString(from) << " (@" << from->username
              << "): " << msgtext << std::endl;
}

void ChatObserver::process(const Message::Ptr& msg) {
    auto chat = msg->chat;
    auto from = msg->from;
    if (from && chat) {
        std::lock_guard<std::mutex> _(m);
        auto it = std::find(observedChatIds.begin(), observedChatIds.end(),
                            chat->id);
        if (it != observedChatIds.end()) {
            if (chat->type != Chat::Type::Supergroup) {
                LOG(LogLevel::WARNING,
                    "Removing chat '%s' from observer: Not a supergroup",
                    chat->title.c_str());
                observedChatIds.erase(it);
                return;
            }
        }
        printChatMsg(msg, from);
    }
}
