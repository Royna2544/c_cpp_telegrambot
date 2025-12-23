#include <AbslLogCompat.hpp>
#include <trivial_helpers/_tgbot.h>

#include <algorithm>
#include <global_handlers/ChatObserver.hpp>
#include <mutex>

using TgBot::Message;

ChatObserver::ChatObserver(TgBotApi::Ptr api) {
    api->onAnyMessage([this](TgBotApi::CPtr, const Message::Ptr& message) {
        if (static_cast<bool>(*this)) {
            process(message);
        }
        return TgBotApi::AnyMessageResult::Handled;
    });
}

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
    else {
        if (msg->text) {
            msgtext = *msg->text;
        } else {
            return;
        }
    }

    fmt::print("[ChatObserveLog][{}]: {} {}\n", msg->chat, msg->from, msgtext);
}

void ChatObserver::process(const Message::Ptr& msg) {
    auto chat = msg->chat;
    auto from = msg->from;
    if (from && chat) {
        std::lock_guard<std::mutex> _(m);
        auto it = std::ranges::find(observedChatIds, chat->id);
        if (it != observedChatIds.end()) {
            if (chat->type != Chat::Type::Supergroup) {
                LOG(WARNING) << "Removing chat '" << chat->title.value()
                             << "' from observer: Not a supergroup";
                observedChatIds.erase(it);
                return;
            }
        }
        printChatMsg(msg, from);
    }
}

bool ChatObserver::startObserving(ChatId chatId) {
    std::lock_guard<std::mutex> _(m);
    if (std::ranges::find(observedChatIds, chatId) == observedChatIds.end()) {
        observedChatIds.push_back(chatId);
        return true;
    } else {
        LOG(WARNING) << "Already observing chat '" << chatId << "'";
        return false;
    }
}

bool ChatObserver::stopObserving(ChatId chatId) {
    std::lock_guard<std::mutex> _(m);
    auto it = std::ranges::find(observedChatIds, chatId);
    if (it != observedChatIds.end()) {
        observedChatIds.erase(it);
        return true;
    } else {
        LOG(WARNING) << "Not observing chat '" << chatId << "'";
        return false;
    }
}

bool ChatObserver::observeAll(bool observe) {
    std::lock_guard<std::mutex> _(m);
    bool prev = observeAllChats;
    observeAllChats = observe;
    if (observeAllChats) {
        LOG(INFO) << "Observing all chats";
    } else {
        LOG(INFO) << "Not observing all chats";
    }
    return prev != observe;
}

ChatObserver::operator bool() const {
    std::lock_guard<std::mutex> _(m);
    return !observedChatIds.empty() || observeAllChats;
}