#include <absl/log/log.h>
#include <api/types/FormatHelper.hpp>

#include <global_handlers/ChatObserver.hpp>
#include <mutex>

ChatObserver::ChatObserver(TgBotApi::Ptr api) {
    api->onAnyMessage([this](TgBotApi::CPtr, const api::types::Message& message) {
        if (static_cast<bool>(*this)) {
            process(message);
        }
        return TgBotApi::AnyMessageResult::Handled;
    });
}

void ChatObserver::printChatMsg(const api::types::Message& msg,
                                const api::types::User& from) {
    std::string msgtext;

    if (msg.sticker)
        msgtext = "Sticker";
    else if (msg.animation)
        msgtext = "GIF";
    else if (!msg.photo)
        msgtext = "Photo";
    else if (msg.document)
        msgtext = "File";
    else if (msg.video)
        msgtext = "Video";
    else if (msg.dice)
        msgtext = "(Dice) " + msg.dice->emoji;
    else {
        if (msg.text) {
            msgtext = *msg.text;
        } else {
            return;
        }
    }

    fmt::print("[ChatObserveLog][{}]: {} {}\n", msg.chat, msg.from, msgtext);
}

void ChatObserver::process(const api::types::Message& msg) {
    const auto& chat = msg.chat;
    auto from = msg.from;
    if (from) {
        std::lock_guard<std::mutex> _(m);
        auto it = observedChatIds.find(chat.id);
        if (it != observedChatIds.end()) {
            if (chat.type != api::types::Chat::Type::Supergroup) {
                LOG(WARNING) << "Removing chat '" << chat.title.value()
                             << "' from observer: Not a supergroup";
                observedChatIds.erase(it);
                return;
            }
        }
        printChatMsg(msg, *from);
    }
}

bool ChatObserver::startObserving(api::types::Chat::id_type chatId) {
    std::lock_guard<std::mutex> _(m);
    auto [it, inserted] = observedChatIds.insert(chatId);
    if (!inserted) {
        LOG(WARNING) << "Already observing chat '" << chatId << "'";
        return false;
    }
    return true;
}

bool ChatObserver::stopObserving(api::types::Chat::id_type chatId) {
    std::lock_guard<std::mutex> _(m);
    if (observedChatIds.erase(chatId) > 0) {
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