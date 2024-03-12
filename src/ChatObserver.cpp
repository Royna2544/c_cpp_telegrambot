#include <ChatObserver.h>
#include <Logging.h>

#include <algorithm>
#include <iostream>

#include "internal/_tgbot.h"

using TgBot::Message;

std::vector<ChatId> gObservedChatIds;
bool gObserveAllChats = false;

static void printChatMsg(const Message::Ptr& msg, const User::Ptr& from) {
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

void processObservers(const Message::Ptr& msg) {
    auto chat = msg->chat;
    auto from = msg->from;
    if (from && chat) {
        auto it = std::find(gObservedChatIds.begin(), gObservedChatIds.end(),
                            chat->id);
        if (it != gObservedChatIds.end()) {
            if (chat->type != Chat::Type::Supergroup) {
                LOG_W("Removing chat '%s' from observer: Not a supergroup",
                      chat->title.c_str());
                gObservedChatIds.erase(it);
                return;
            }
        }
        printChatMsg(msg, from);
    }
}
