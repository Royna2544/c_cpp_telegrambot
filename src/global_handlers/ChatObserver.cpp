#include <absl/log/log.h>
#include <trivial_helpers/_tgbot.h>

#include <algorithm>
#include <global_handlers/ChatObserver.hpp>
#include <iostream>
#include <mutex>

using TgBot::Message;

void ChatObserver::printChatMsg(const Message::Ptr& msg,
                                const User::Ptr& from) {
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

    fmt::print("[ChatObserveLog][{}]: {}\n", msg->chat, msg->from, msgtext);
}

void ChatObserver::process(const Message::Ptr& msg) {
    auto chat = msg->chat;
    auto from = msg->from;
    if (from && chat) {
        std::lock_guard<std::mutex> _(m);
        auto it =
            std::find(observedChatIds.begin(), observedChatIds.end(), chat->id);
        if (it != observedChatIds.end()) {
            if (chat->type != Chat::Type::Supergroup) {
                LOG(WARNING) << "Removing chat '" << chat->title
                             << "' from observer: Not a supergroup";
                observedChatIds.erase(it);
                return;
            }
        }
        printChatMsg(msg, from);
    }
}

DECLARE_CLASS_INST(ChatObserver);